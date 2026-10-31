#include "lcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../customizedReader/binaryUtils.h"
#include "../readerInternal.h"
#include "libCacheSim/llmCostModel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reader state, held in reader->reader_params. The flat reader hands out one
 * request_t per block, so it has to remember where it is inside a request. */
typedef struct {
  lcp_trace_stat_t stat;
  size_t req_size;
  llm_cost_model_e cost_model;

  /* streaming state, cleared by lcp_reset_reader() */
  int64_t blocks_left; /* in the current request, flat reading only */
  int64_t block_pos;   /* 0-based position of the next block in its request */
  int64_t clock_time;  /* current request's completion time, seconds */
  int64_t next_access; /* decoded next access of the previous block in the
                          current request; 0 before its first block */
  int64_t n_req_read;
  int64_t n_block_read;
  uint64_t max_block_id; /* for the density check */
  size_t released;       /* bytes of a plain file's mapping handed back */
} lcp_params_t;

/* A plain file is mmap'd whole, and every page read would stay resident until
 * the reader closes, so memory would grow to the file's size. Pages behind the
 * read position are never needed again in a forward pass, so hand them back
 * every LCP_RELEASE_STEP bytes: reading then holds a bounded window. A rewind
 * simply faults them in again. zstd input streams through its own buffer. */
#define LCP_RELEASE_STEP ((size_t)64 << 20)

static void _release_consumed(reader_t *reader, lcp_params_t *params) {
  if (reader->is_zstd_file || reader->mapped_file == NULL) return;
  const size_t page = (size_t)sysconf(_SC_PAGESIZE);
  const size_t done = reader->mmap_offset / page * page;
  if (done < params->released + LCP_RELEASE_STEP) return;
  madvise(reader->mapped_file + params->released, done - params->released,
          MADV_DONTNEED);
  params->released = done;
}

static bool _verify_lcp_header(const lcp_trace_header_t *header) {
  if (header->start_magic != LCP_TRACE_MAGIC ||
      header->end_magic != LCP_TRACE_MAGIC) {
    ERROR("not an lcp trace: magic is 0x%llx/0x%llx, expected 0x%llx\n",
          (unsigned long long)header->start_magic,
          (unsigned long long)header->end_magic,
          (unsigned long long)LCP_TRACE_MAGIC);
    return false;
  }
  if (lcp_req_size(header->version) == 0) {
    ERROR("unsupported lcp version %llu, this build reads up to %d\n",
          (unsigned long long)header->version, LCP_CURR_VERSION);
    return false;
  }
  const lcp_trace_stat_t *stat = &header->stat;
  if (stat->n_req < 0 || stat->n_block < 0 || stat->n_unique_block < 0 ||
      stat->n_session < 0 || stat->n_user < 0 || stat->max_blocks_per_req < 0 ||
      stat->n_type < 0 || stat->n_type > LCP_MAX_TYPE) {
    ERROR(
        "invalid lcp stat: n_req %lld, n_block %lld, n_unique_block %lld, "
        "n_session %lld, n_user %lld, max_blocks_per_req %lld, n_type %lld\n",
        (long long)stat->n_req, (long long)stat->n_block,
        (long long)stat->n_unique_block, (long long)stat->n_session,
        (long long)stat->n_user, (long long)stat->max_blocks_per_req,
        (long long)stat->n_type);
    return false;
  }
  return true;
}

int lcpReader_setup(reader_t *reader) {
  char *data = read_bytes(reader, sizeof(lcp_trace_header_t));
  if (data == NULL) {
    ERROR("lcp trace %s is shorter than its header\n", reader->trace_path);
    exit(1);
  }
  const lcp_trace_header_t *header = (const lcp_trace_header_t *)data;
  if (!_verify_lcp_header(header)) {
    exit(1);
  }

  lcp_params_t *params = calloc(1, sizeof(lcp_params_t));
  memcpy(&params->stat, &header->stat, sizeof(lcp_trace_stat_t));
  params->req_size = lcp_req_size(header->version);
  params->cost_model = (llm_cost_model_e)reader->init_params.llm_cost_model;
  reader->reader_params = params;

  reader->trace_type = LCP_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->trace_start_offset = sizeof(lcp_trace_header_t);
  reader->obj_id_is_num = true;
  /* the flat reader hands out one request_t per block */
  reader->n_total_req = params->stat.n_block;
  reader->item_size = sizeof(lcp_block_t);

  /* The header fixes the file's length exactly, so an uncompressed file that
   * is cut short or carries trailing bytes is caught here, before a
   * simulation runs on part of it. A zstd file is checked as it is read. */
  if (!reader->is_zstd_file) {
    const size_t expected = sizeof(lcp_trace_header_t) +
                            (size_t)params->stat.n_req * params->req_size +
                            (size_t)params->stat.n_block * sizeof(lcp_block_t);
    if (reader->file_size != expected) {
      ERROR(
          "lcp trace %s is %zu bytes, but its header describes %zu "
          "(%lld requests, %lld blocks): the file is truncated or corrupt\n",
          reader->trace_path, reader->file_size, expected,
          (long long)params->stat.n_req, (long long)params->stat.n_block);
      exit(1);
    }
  }
  return 0;
}

void lcp_reset_reader(reader_t *reader) {
  lcp_params_t *params = (lcp_params_t *)reader->reader_params;
  params->blocks_left = 0;
  params->block_pos = 0;
  params->clock_time = 0;
  params->next_access = 0;
  params->n_req_read = 0;
  params->n_block_read = 0;
  params->max_block_id = 0;
  params->released = 0;
}

const lcp_trace_stat_t *lcp_get_stat(const reader_t *reader) {
  return &((const lcp_params_t *)reader->reader_params)->stat;
}

/* Read the next request record into *rec. False at the end of the trace, which
 * the header's request count decides. */
static bool _read_record(reader_t *reader, lcp_params_t *params,
                         lcp_req_v1_t *rec) {
  if (params->n_req_read == params->stat.n_req) {
    if (params->n_block_read != params->stat.n_block) {
      ERROR("lcp trace %s holds %lld blocks, but its header says %lld\n",
            reader->trace_path, (long long)params->n_block_read,
            (long long)params->stat.n_block);
      exit(1);
    }
    return false;
  }
  char *data = read_bytes(reader, params->req_size);
  if (data == NULL) {
    ERROR("lcp trace %s ends after %lld of %lld requests: truncated\n",
          reader->trace_path, (long long)params->n_req_read,
          (long long)params->stat.n_req);
    exit(1);
  }
  /* every version starts with the v1 record */
  memcpy(rec, data, sizeof(*rec));
  if ((int64_t)rec->n_blocks > params->stat.max_blocks_per_req ||
      (int64_t)rec->session_id > params->stat.n_session ||
      (int64_t)rec->user_id > params->stat.n_user || rec->type_id < -1 ||
      rec->type_id >= params->stat.n_type) {
    ERROR(
        "lcp trace %s: request %lld has %u blocks, session %u, user %u, type "
        "%d, but the header allows at most %lld, %lld, %lld and %lld\n",
        reader->trace_path, (long long)params->n_req_read + 1, rec->n_blocks,
        rec->session_id, rec->user_id, rec->type_id,
        (long long)params->stat.max_blocks_per_req,
        (long long)params->stat.n_session, (long long)params->stat.n_user,
        (long long)params->stat.n_type - 1);
    exit(1);
  }
  if (rec->start_time_us < 0 || rec->start_time_us > rec->complete_time_us) {
    ERROR(
        "lcp trace %s: request %lld starts at %lld us and completes at %lld "
        "us, but must start at or after 0 and no later than it completes\n",
        reader->trace_path, (long long)params->n_req_read + 1,
        (long long)rec->start_time_us, (long long)rec->complete_time_us);
    exit(1);
  }
  params->n_req_read += 1;
  params->next_access = 0;
  _release_consumed(reader, params);
  return true;
}

/* Read the next block entry: its id, and its next access decoded from the
 * stored difference (-1 = never). */
static void _read_block(reader_t *reader, lcp_params_t *params, uint64_t *id,
                        int64_t *next_access) {
  char *data = read_bytes(reader, sizeof(lcp_block_t));
  if (data == NULL) {
    ERROR("lcp trace %s ends inside request %lld: truncated\n",
          reader->trace_path, (long long)params->n_req_read);
    exit(1);
  }
  lcp_block_t b;
  memcpy(&b, data, sizeof(b));

  /* Ids are dense in first-appearance order, so a new id is exactly one past
   * the largest seen. Checking costs one compare and catches a file whose ids
   * were not produced by the positional encoding. */
  if (b.block_id == 0 || b.block_id > params->max_block_id + 1) {
    ERROR(
        "lcp trace %s: block id %llu follows largest id %llu, but ids "
        "must be assigned 1, 2, 3, ... in order of first appearance\n",
        reader->trace_path, (unsigned long long)b.block_id,
        (unsigned long long)params->max_block_id);
    exit(1);
  }
  if (b.block_id > params->max_block_id) params->max_block_id = b.block_id;
  params->n_block_read += 1;

  /* A next access lies after the block's own position and inside the trace.
   * A wrong difference throws every later value in its request off, so a file
   * that stores values rather than differences soon fails this. Unsigned, so
   * a corrupt difference wraps instead of overflowing. */
  params->next_access =
      (int64_t)((uint64_t)params->next_access + (uint64_t)b.next_access_delta);
  if (params->next_access != -1 &&
      (params->next_access <= params->n_block_read ||
       params->next_access > params->stat.n_block)) {
    ERROR(
        "lcp trace %s: block %lld's next access decodes to %lld, but must be "
        "-1 or in (%lld, %lld]\n",
        reader->trace_path, (long long)params->n_block_read,
        (long long)params->next_access, (long long)params->n_block_read,
        (long long)params->stat.n_block);
    exit(1);
  }
  *id = b.block_id;
  *next_access = params->next_access;
}

/**
 * Read one BLOCK as a request_t. Request records are consumed transparently:
 * when the current request runs out, the next record is read and its
 * completion time carried across that request's blocks.
 *
 * obj_cost is derived here from the block's position in its request under the
 * model chosen with the `cost-model` reader parameter, since the trace stores
 * no cost. clock_time is in seconds, libCacheSim's convention; the file keeps
 * microseconds.
 */
int lcp_read_one_req(reader_t *reader, request_t *req) {
  lcp_params_t *params = (lcp_params_t *)reader->reader_params;

  while (params->blocks_left == 0) {
    lcp_req_v1_t rec;
    if (!_read_record(reader, params, &rec)) {
      req->valid = false;
      return 1;
    }
    params->blocks_left = (int64_t)rec.n_blocks;
    params->block_pos = 0;
    params->clock_time = rec.complete_time_us / 1000000;
  }

  uint64_t id;
  int64_t next_access;
  _read_block(reader, params, &id, &next_access);

  req->clock_time = params->clock_time;
  req->obj_id = id;
  req->obj_size = 1;
  req->obj_cost = llm_block_cost(params->cost_model, params->block_pos);
  req->next_access_vtime = next_access < 0 ? MAX_REUSE_DISTANCE : next_access;

  params->blocks_left -= 1;
  params->block_pos += 1;
  return 0;
}

int lcp_read_trace_req(reader_t *reader, lcp_req_v1_t *rec, uint64_t *block_ids,
                       int64_t *next_access_vtime) {
  lcp_params_t *params = (lcp_params_t *)reader->reader_params;
  if (!_read_record(reader, params, rec)) return 1;
  for (uint32_t i = 0; i < rec->n_blocks; i++) {
    _read_block(reader, params, &block_ids[i], &next_access_vtime[i]);
  }
  return 0;
}

void lcp_print_trace_stat(reader_t *reader) {
  const lcp_params_t *params = (const lcp_params_t *)reader->reader_params;
  const lcp_trace_stat_t *stat = &params->stat;
  const double duration_s =
      (double)(stat->last_complete_time_us - stat->first_complete_time_us) /
      1e6;

  printf(
      "trace stat: n_req %lld, n_block %lld, n_unique_block %lld, "
      "n_session %lld, n_user %lld, max %lld blocks/request, %lld "
      "tokens/block\n",
      (long long)stat->n_req, (long long)stat->n_block,
      (long long)stat->n_unique_block, (long long)stat->n_session,
      (long long)stat->n_user, (long long)stat->max_blocks_per_req,
      (long long)stat->block_size_tokens);
  printf(
      "completion times %lld us to %lld us, duration %.0lf seconds "
      "%.2lf days\n",
      (long long)stat->first_complete_time_us,
      (long long)stat->last_complete_time_us, duration_s,
      duration_s / (24 * 3600.0));
  printf("request types:");
  for (int64_t i = 0; i < stat->n_type; i++) {
    printf(" %.*s", LCP_TYPE_NAME_LEN, stat->type_names[i]);
  }
  printf("\n");
}

#ifdef __cplusplus
}
#endif

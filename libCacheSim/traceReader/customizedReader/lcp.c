#include "lcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  int64_t n_req_read;
  int64_t n_block_read;
  uint64_t max_block_id; /* for the density check */
} lcp_params_t;

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
      stat->n_session < 0 || stat->max_blocks_per_req < 0 || stat->n_type < 0 ||
      stat->n_type > LCP_MAX_TYPE) {
    ERROR(
        "invalid lcp stat: n_req %lld, n_block %lld, n_unique_block %lld, "
        "n_session %lld, max_blocks_per_req %lld, n_type %lld\n",
        (long long)stat->n_req, (long long)stat->n_block,
        (long long)stat->n_unique_block, (long long)stat->n_session,
        (long long)stat->max_blocks_per_req, (long long)stat->n_type);
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
  params->n_req_read = 0;
  params->n_block_read = 0;
  params->max_block_id = 0;
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
      (int64_t)rec->session_id > params->stat.n_session || rec->type_id < -1 ||
      rec->type_id >= params->stat.n_type) {
    ERROR(
        "lcp trace %s: request %lld has %u blocks, session %u, type %d, "
        "but the header allows at most %lld, %lld and %lld\n",
        reader->trace_path, (long long)params->n_req_read + 1, rec->n_blocks,
        rec->session_id, rec->type_id,
        (long long)params->stat.max_blocks_per_req,
        (long long)params->stat.n_session, (long long)params->stat.n_type - 1);
    exit(1);
  }
  params->n_req_read += 1;
  return true;
}

static void _read_block(reader_t *reader, lcp_params_t *params,
                        lcp_block_t *b) {
  char *data = read_bytes(reader, sizeof(lcp_block_t));
  if (data == NULL) {
    ERROR("lcp trace %s ends inside request %lld: truncated\n",
          reader->trace_path, (long long)params->n_req_read);
    exit(1);
  }
  memcpy(b, data, sizeof(*b));

  /* Ids are dense in first-appearance order, so a new id is exactly one past
   * the largest seen. Checking costs one compare and catches a file whose ids
   * were not produced by the positional encoding. */
  if (b->block_id == 0 || b->block_id > params->max_block_id + 1) {
    ERROR(
        "lcp trace %s: block id %llu follows largest id %llu, but ids "
        "must be assigned 1, 2, 3, ... in order of first appearance\n",
        reader->trace_path, (unsigned long long)b->block_id,
        (unsigned long long)params->max_block_id);
    exit(1);
  }
  if (b->block_id > params->max_block_id) params->max_block_id = b->block_id;
  params->n_block_read += 1;
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
    params->clock_time = rec.timestamp_us / 1000000;
  }

  lcp_block_t b;
  _read_block(reader, params, &b);

  req->clock_time = params->clock_time;
  req->obj_id = b.block_id;
  req->obj_size = 1;
  req->obj_cost = llm_block_cost(params->cost_model, params->block_pos);
  req->next_access_vtime =
      b.next_access_vtime < 0 ? MAX_REUSE_DISTANCE : b.next_access_vtime;

  params->blocks_left -= 1;
  params->block_pos += 1;
  return 0;
}

int lcp_read_trace_req(reader_t *reader, lcp_req_v1_t *rec,
                       lcp_block_t *blocks) {
  lcp_params_t *params = (lcp_params_t *)reader->reader_params;
  if (!_read_record(reader, params, rec)) return 1;
  for (uint32_t i = 0; i < rec->n_blocks; i++) {
    _read_block(reader, params, &blocks[i]);
  }
  return 0;
}

void lcp_print_trace_stat(reader_t *reader) {
  const lcp_params_t *params = (const lcp_params_t *)reader->reader_params;
  const lcp_trace_stat_t *stat = &params->stat;
  const double duration_s =
      (double)(stat->end_timestamp_us - stat->start_timestamp_us) / 1e6;

  printf(
      "trace stat: n_req %lld, n_block %lld, n_unique_block %lld, "
      "n_session %lld, max %lld blocks/request, %lld tokens/block\n",
      (long long)stat->n_req, (long long)stat->n_block,
      (long long)stat->n_unique_block, (long long)stat->n_session,
      (long long)stat->max_blocks_per_req, (long long)stat->block_size_tokens);
  printf(
      "start time %lld us, end time %lld us, duration %.0lf seconds "
      "%.2lf days\n",
      (long long)stat->start_timestamp_us, (long long)stat->end_timestamp_us,
      duration_s, duration_s / (24 * 3600.0));
  printf("request types:");
  for (int64_t i = 0; i < stat->n_type; i++) {
    printf(" %.*s", LCP_TYPE_NAME_LEN, stat->type_names[i]);
  }
  printf("\n");
}

#ifdef __cplusplus
}
#endif

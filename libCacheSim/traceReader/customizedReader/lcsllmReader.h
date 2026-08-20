#pragma once

// The lcsllm trace format: a binary, request-oriented trace for LLM
// prefix-cache simulation. See doc/lcsllm_format.md for the full spec.
//
//   [file header 8192 B]
//   [request header 48 B][block entry 16 B] x n_blocks
//   [request header 48 B][block entry 16 B] x n_blocks
//   ...
//
// Request-oriented because a prefix cache's behaviour depends on which blocks
// belong to the same request; a format that emits one block at a time cannot
// express that. Variable-length records are fine here: read_bytes() takes an
// arbitrary size, so the whole format streams without an offset table.
//
// This header also provides the *flat* reader, which walks the same file and
// hands out one request_t per block so cachesim/traceAnalyzer/mrcProfiler work
// unchanged. prefixsim reads the file request-at-a-time instead.

#include <inttypes.h>
#include <string.h>

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCSLLM_MAGIC 0x6c63736c6c6d0001ULL
#define LCSLLM_CURR_VERSION 1
#define LCSLLM_N_TYPE 8
#define LCSLLM_TYPE_LEN 16

/** how block ids in the file were derived */
typedef enum {
  /** The source trace's per-block hash, carried through unchanged. NO LONGER
   *  WRITTEN and rejected on read: such ids are not positionally encoded, so
   *  one id can denote two different cache objects. Kept as a named value so an
   *  older file is diagnosed rather than silently misread. */
  LCSLLM_BLOCK_ID_RAW = 0,
  /** Positional encoding: the id is a hash folded over the whole path from the
   *  root up to and including this block, so two blocks share an id only when
   *  the block *and its entire prefix* are identical. The only legal value. */
  LCSLLM_BLOCK_ID_PREFIX = 1,
} lcsllm_block_id_kind_e;

/******************************************************************************/
/**                        file header (8192 bytes)                          **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcsllm_header {
  uint64_t start_magic;
  uint64_t version;

  int64_t n_requests;
  int64_t n_blocks;
  int64_t n_unique_blocks;
  int64_t max_blocks_per_request;
  int64_t start_time_us;
  int64_t end_time_us;

  int32_t block_size_tokens;  /**< tokens per KV block, e.g. 16 */
  uint8_t block_id_kind;      /**< lcsllm_block_id_kind_e */
  uint8_t has_next_access;    /**< 0 if next_access_vtime is -1 throughout */
  uint8_t n_types;
  uint8_t unused_pad;
  char type_names[LCSLLM_N_TYPE][LCSLLM_TYPE_LEN];

  uint64_t unused[998];
  uint64_t end_magic;
} lcsllm_header_t;
typedef char static_assert_lcsllm_header_size
    [(sizeof(struct lcsllm_header) == 8192) ? 1 : -1];

/******************************************************************************/
/**                       request header (48 bytes)                          **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcsllm_req_header {
  uint64_t chat_id;
  int64_t parent_chat_id;  /**< -1 if the conversation has no parent */
  int64_t timestamp_us;    /**< arrival time, microseconds */
  uint32_t n_blocks;
  uint32_t input_length;   /**< prompt tokens */
  uint32_t output_length;  /**< generated tokens */
  uint32_t turn;
  uint8_t type_id;         /**< index into header.type_names */
  uint8_t reserved[7];
} lcsllm_req_header_t;
typedef char static_assert_lcsllm_req_header_size
    [(sizeof(struct lcsllm_req_header) == 48) ? 1 : -1];

/******************************************************************************/
/**                         block entry (16 bytes)                           **/
/**   blocks appear in prefix order: index 0 is the root of the shared path  **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcsllm_block {
  uint64_t block_id;
  /** 1-based index of the next access in the forward-flattened trace, or -1 */
  int64_t next_access_vtime;
} lcsllm_block_t;
typedef char static_assert_lcsllm_block_size
    [(sizeof(struct lcsllm_block) == 16) ? 1 : -1];

/******************************************************************************/
/**                              flat reader                                 **/
/******************************************************************************/

static inline int lcsllmReader_setup(reader_t *reader) {
  char *data = read_bytes(reader, sizeof(lcsllm_header_t));
  if (data == NULL) {
    ERROR("lcsllm trace is shorter than its header\n");
    return 1;
  }

  lcsllm_header_t *header = (lcsllm_header_t *)data;
  if (header->start_magic != LCSLLM_MAGIC ||
      header->end_magic != LCSLLM_MAGIC) {
    ERROR(
        "not an lcsllm trace: magic is 0x%llx/0x%llx, expected 0x%llx. "
        "The block-only lcsllm layout had no header and is no longer "
        "supported; regenerate the trace with traceConvLLM.\n",
        (unsigned long long)header->start_magic,
        (unsigned long long)header->end_magic,
        (unsigned long long)LCSLLM_MAGIC);
    return 1;
  }
  if (header->version != LCSLLM_CURR_VERSION) {
    ERROR("unsupported lcsllm version %llu, this build reads %d\n",
          (unsigned long long)header->version, LCSLLM_CURR_VERSION);
    return 1;
  }
  /* Positional encoding is a guarantee of the format, not a property to be
   * discovered by the consumer. A prefix cache keys on "this block reached
   * through this exact prefix": if two distinct paths share an id, they collapse
   * into one cache object, inflating the hit ratio and -- under a
   * position-dependent cost model -- giving one object two different costs. A
   * consumer cannot repair that after the fact, because the predecessor
   * information is gone by then. So it is refused here. */
  if (header->block_id_kind != LCSLLM_BLOCK_ID_PREFIX) {
    ERROR(
        "lcsllm trace has block_id_kind=%u, but the format requires %d "
        "(positional encoding: an id is a hash over the whole path from the "
        "root, so two blocks share an id only when the block and its prefix "
        "are identical). Regenerate it with traceConvLLM.\n",
        (unsigned)header->block_id_kind, LCSLLM_BLOCK_ID_PREFIX);
    return 1;
  }

  reader->trace_type = LCSLLM_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->trace_start_offset = sizeof(lcsllm_header_t);
  reader->obj_id_is_num = true;
  /* the flat reader hands out one request_t per block */
  reader->n_total_req = header->n_blocks;
  reader->item_size = sizeof(lcsllm_block_t);
  /* no request is open yet */
  reader->lcsllm_blocks_left = 0;
  reader->last_req_clock_time = 0;

  return 0;
}

/**
 * Read one BLOCK as a request_t. Request boundaries are consumed transparently:
 * when the current request runs out, the next request header is read and its
 * timestamp carried across that request's blocks.
 *
 * Counted in reader->lcsllm_blocks_left rather than n_req_left: read_one_req()
 * short-circuits on n_req_left to replay one object several times, which would
 * hand back the same block instead of advancing to the next one.
 */
static inline int lcsllm_read_one_req(reader_t *reader, request_t *req) {
  while (reader->lcsllm_blocks_left == 0) {
    char *rec = read_bytes(reader, sizeof(lcsllm_req_header_t));
    if (rec == NULL) {
      req->valid = FALSE;
      return 1;
    }
    lcsllm_req_header_t *rh = (lcsllm_req_header_t *)rec;
    reader->lcsllm_blocks_left = (int64_t)rh->n_blocks;
    reader->last_req_clock_time = rh->timestamp_us;
    /* a zero-block request is legal but carries nothing; skip it */
  }

  char *record = read_bytes(reader, sizeof(lcsllm_block_t));
  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }
  lcsllm_block_t *b = (lcsllm_block_t *)record;

  req->clock_time = reader->last_req_clock_time;
  req->obj_id = b->block_id;
  req->obj_size = 1;
  req->next_access_vtime =
      b->next_access_vtime < 0 ? MAX_REUSE_DISTANCE : b->next_access_vtime;
  reader->lcsllm_blocks_left -= 1;

  return 0;
}

#ifdef __cplusplus
}
#endif

//
// The lcp ("libCacheSim prefix") trace format: libCacheSim's binary,
// request-oriented trace for LLM prefix-cache simulation. See doc/lcp_format.md
// for the spec.
//
//   [header 8192 B]
//   [request record][block entry] x n_blocks
//   [request record][block entry] x n_blocks
//   ...
//
// Modelled on lcs: a fixed 8 KiB header holding magic, version and a stat
// block, and one record struct per version. A later version embeds the
// previous record as `base` and appends fields, so a reader handles every
// version by reading the common prefix.
//
// Unlike lcs, records vary in length -- a request is followed by its blocks --
// so the file has no fixed item size; read_bytes() takes an arbitrary size,
// and the format streams without an offset table. Every future-knowledge field
// a simulator needs (block and session next access) is stored, so one forward
// pass over the file is enough to simulate it.

#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include "libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCP_TRACE_MAGIC 0x6c63700000000001ULL
#define LCP_CURR_VERSION 1
#define LCP_CURR_STAT_VERSION 1
#define LCP_MAX_TYPE 32
#define LCP_TYPE_NAME_LEN 32

/******************************************************************************/
/**                     lcp trace stat (1000 * 8 bytes)                      **/
/**   Fields added in a later stat version read as 0 in older traces, so     **/
/**   0 must never be a meaningful value for a new field.                    **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcp_trace_stat {
  int64_t version; /**< stat version */
  int64_t n_req;
  int64_t n_block;        /**< block accesses, summed over requests */
  int64_t n_unique_block; /**< block ids are exactly 1..n_unique_block */
  int64_t n_session;      /**< session ids are exactly 1..n_session; 0 = none */
  int64_t max_blocks_per_req;
  int64_t start_timestamp_us;
  int64_t end_timestamp_us;
  int64_t block_size_tokens; /**< tokens per KV block */
  int64_t n_type;            /**< entries used in type_names */
  /** request-type strings, indexed by a request's type_id */
  char type_names[LCP_MAX_TYPE][LCP_TYPE_NAME_LEN];

  int64_t unused[862];
} lcp_trace_stat_t;
typedef char static_assert_lcp_stat_size
    [(sizeof(struct lcp_trace_stat) == 1000 * 8) ? 1 : -1];

/******************************************************************************/
/**                lcp header (8192 bytes), laid out as lcs's                **/
/**   version decides the request record layout                              **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcp_trace_header {
  uint64_t start_magic;
  uint64_t version;
  lcp_trace_stat_t stat;

  uint64_t unused[21];
  uint64_t end_magic;
} lcp_trace_header_t;
typedef char static_assert_lcp_header_size
    [(sizeof(struct lcp_trace_header) == 1024 * 8) ? 1 : -1];

/******************************************************************************/
/**                     v1 request record (56 bytes)                         **/
/**   -1 marks a field the source trace did not carry                        **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcp_req_v1 {
  int64_t timestamp_us;   /**< completion time, microseconds */
  int64_t chat_id;        /**< source trace's chat_id */
  int64_t parent_chat_id; /**< chat_id this request follows up; -1 none */
  /** 1-based index (in file order) of the next request with the same
   *  session_id, -1 = none. The session-level next_access_vtime. */
  int64_t session_next_req;
  uint32_t session_id;   /**< 1..n_session, first-appearance order; 0 none */
  uint32_t n_blocks;     /**< block entries that follow */
  int32_t input_length;  /**< prompt tokens */
  int32_t output_length; /**< generated tokens */
  int32_t turn;          /**< turn index within the session */
  int32_t type_id;       /**< index into stat.type_names */
} lcp_req_v1_t;
typedef char
    static_assert_lcp_req_v1_size[(sizeof(struct lcp_req_v1) == 56) ? 1 : -1];

/******************************************************************************/
/**                        block entry (16 bytes)                            **/
/**   blocks appear in prefix order: index 0 is the first block of the       **/
/**   prompt. block_id is positionally encoded and dense: two entries share  **/
/**   an id iff the block and its entire prefix are identical, and ids are   **/
/**   assigned 1, 2, 3, ... in order of first appearance.                    **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcp_block {
  uint64_t block_id;
  /** 1-based index of the next access in the flattened trace, -1 = never */
  int64_t next_access_vtime;
} lcp_block_t;
typedef char
    static_assert_lcp_block_size[(sizeof(struct lcp_block) == 16) ? 1 : -1];

/** size in bytes of one request record of the given version, 0 if unknown */
static inline size_t lcp_req_size(uint64_t version) {
  return version == 1 ? sizeof(lcp_req_v1_t) : 0;
}

int lcpReader_setup(reader_t *reader);

/** Flat reading: one BLOCK per request_t, for cachesim and the other
 *  reader_t consumers. */
int lcp_read_one_req(reader_t *reader, request_t *req);

/** Request-level reading, for prefixsim: the next request's record (the v1
 *  prefix of any version) into *rec and its block entries, stored values
 *  unchanged, into blocks[0 .. rec->n_blocks). `blocks` must hold
 *  stat.max_blocks_per_req entries. Returns 0, or 1 at end of trace; exits on
 *  a truncated or malformed file. Do not mix with lcp_read_one_req on one
 *  reader. */
int lcp_read_trace_req(reader_t *reader, lcp_req_v1_t *rec,
                       lcp_block_t *blocks);

/** The stat block from the trace's header. */
const lcp_trace_stat_t *lcp_get_stat(const reader_t *reader);

void lcp_reset_reader(reader_t *reader);

void lcp_print_trace_stat(reader_t *reader);

#ifdef __cplusplus
}
#endif

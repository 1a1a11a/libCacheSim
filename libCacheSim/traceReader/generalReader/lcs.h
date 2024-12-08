//
// Description: This file defines the lcs (libCacheSim) trace format for libCacheSim.
//
// A lcs trace file consists of a header and a sequence of requests.
// The header is 1024 bytes, and the request is 24 bytes for v1 and 28 bytes for v2.
// The header contains the trace statistics
// The request contains the request information
// The trace stat is defined in the lcs_trace_stat struct.
// The request format is defined in the lcs_req_v1_t and lcs_req_v2_t structs.

// The LCSReader_setup function sets up the reader for reading lcs traces.
// The lcs_read_one_req function reads one request from the trace file.

#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCS_TRACE_START_MAGIC 0x123456789abcdef0
#define LCS_TRACE_END_MAGIC 0x123456789abcdef0

#define MAX_LCS_VERSION 2

/******************************************************************************/
/**                    lcs trace stat header (512 bytes)                     **/
/**   this stores information of the trace as part of the lcs trace header   **/
/**   note that some fields that are added later will have 0 value           **/
/**         if the trace was generated before the format update              **/
/**         so we should avoid using 0 as the  default value                 **/
/******************************************************************************/
typedef struct lcs_trace_stat {
  int64_t version;

  /**** v1 ****/
  int64_t n_req;       // number of requests
  int64_t n_obj;       // number of objects
  int64_t n_req_byte;  // number of bytes requested
  int64_t n_obj_byte;  // number of bytes of objects

  int64_t start_timestamp;  // in seconds
  int64_t end_timestamp;    // in seconds

  int64_t n_read;    // number of read requests
  int64_t n_write;   // number of write requests
  int64_t n_delete;  // number of delete requests

  int32_t n_tenant;

  // block cache specific
  int32_t block_size;    // used in block trace, block size in bytes
  int64_t n_uniq_block;  // number of unique blocks

  // key-value cache and object cache specific
  int32_t n_ttl;
  int32_t smallest_ttl;
  int32_t largest_ttl;

  int32_t time_unit;   // 1: seconds, 2: milliseconds, 3: microseconds, 4: nanoseconds
  int32_t trace_type;  // 1: block, 2: key-value, 3: object, 4: file
  int32_t unused1;

  int64_t unused[49];
} lcs_trace_stat_t;
// assert the struct size at compile time
typedef char static_assert_lcs_trace_stat_size[(sizeof(struct lcs_trace_stat) == 512) ? 1 : -1];

/******************************************************************************/
/**                    lcs trace format header (1024 bytes)                  **/
/**       start_magic and end_magic is to make sure the trace is valid       **/
/**   the main field is                                                      **/
/**         1) version, which decides the request format                     **/
/**         2) stat, which contains the trace statistics                     **/
/******************************************************************************/
typedef struct lcs_trace_header {
  uint64_t start_magic;
  // the version of lcs trace, see lcs_v1, lcs_v2, etc.
  uint64_t version;
  struct lcs_trace_stat stat;

  uint64_t unused[61];
  uint64_t end_magic;
} lcs_trace_header_t;
// assert the struct size at compile time
typedef char static_assert_lcs_trace_header_size[(sizeof(struct lcs_trace_header) == 1024) ? 1 : -1];

/******************************************************************************/
/**       v1 is the simplest trace format (same as oracleGeneral)            **/
/** it only contains the clock time, obj_id, obj_size, and next_access_vtime **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v1 {
  uint32_t clock_time;
  // this is the hash of key in key-value cache
  // or the logical block address in block cache
  uint64_t obj_id;
  uint32_t obj_size;
  int64_t next_access_vtime;
} lcs_req_v1_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v1_size[(sizeof(struct lcs_req_v1) == 24) ? 1 : -1];

/******************************************************************************/
/**              v2 has more fields, operation and tenant                    **/
/******************************************************************************/
//
// specifically we add operation and ns
typedef struct __attribute__((packed)) lcs_req_v2 {
  uint32_t clock_time;
  // this is the hash of key in key-value cache
  // or the logical block address in block cache
  uint64_t obj_id;
  uint32_t obj_size;
  uint32_t op : 8;
  uint32_t tenant : 24;
  int64_t next_access_vtime;
} lcs_req_v2_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v2_size[(sizeof(struct lcs_req_v2) == 28) ? 1 : -1];




int lcsReader_setup(reader_t *reader);

int lcs_read_one_req(reader_t *reader, request_t *req);

void lcs_print_trace_stat(reader_t *reader);

#ifdef __cplusplus
}
#endif

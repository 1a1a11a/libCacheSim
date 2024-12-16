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
#define CURR_STAT_VERSION 1
#define N_MOST_COMMON 16

/******************************************************************************/
/**                  lcs trace stat header (1000 * 8 bytes)                  **/
/**   this stores information of the trace as part of the lcs trace header   **/
/**   note that some fields that are added later will have 0 value           **/
/**         if the trace was generated before the format update              **/
/**         so we should avoid using 0 as the  default value                 **/
/******************************************************************************/
typedef struct lcs_trace_stat {
  int64_t version;     // version of the stat
  int64_t n_req;       // number of requests
  int64_t n_obj;       // number of objects
  int64_t n_req_byte;  // number of bytes requested
  int64_t n_obj_byte;  // number of unique bytes

  int64_t start_timestamp;  // in seconds
  int64_t end_timestamp;    // in seconds

  int64_t n_read;    // number of read requests
  int64_t n_write;   // number of write requests
  int64_t n_delete;  // number of delete requests
  // 10 * 8 bytes so far

  // object size
  int64_t smallest_obj_size;
  int64_t largest_obj_size;
  int64_t most_common_obj_sizes[N_MOST_COMMON];
  float most_common_obj_size_ratio[N_MOST_COMMON];
  // (10 + 26) * 8 bytes so far

  // popularity
  // the request count of the most popular objects
  int64_t highest_freq[N_MOST_COMMON];
  // unpopular objects:
  int32_t most_common_freq[N_MOST_COMMON];
  float most_common_freq_ratio[N_MOST_COMMON];
  // zipf alpha
  double skewness;
  // (10 + 26 + 33) * 8 bytes so far

  // tenant info
  int32_t n_tenant;
  int32_t most_common_tenants[N_MOST_COMMON];
  float most_common_tenant_ratio[N_MOST_COMMON];
  // (10 + 26 + 33 + 16.5) * 8 bytes so far

  // key-value cache and object cache specific
  int32_t n_ttl;
  int32_t smallest_ttl;
  int32_t largest_ttl;
  int32_t most_common_ttls[N_MOST_COMMON];
  float most_common_ttl_ratio[N_MOST_COMMON];
  // (10 + 26 + 33 + 16.5 + 17.5) * 8 bytes so far

  int64_t unused[897];
} __attribute__((packed)) lcs_trace_stat_t;
// assert the struct size at compile time
typedef char static_assert_lcs_trace_stat_size[(sizeof(struct lcs_trace_stat) == 1000 * 8) ? 1 : -1];

/******************************************************************************/
/**                    lcs trace format header (8192 bytes)                  **/
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

  uint64_t unused[21];
  uint64_t end_magic;
} __attribute__((packed)) lcs_trace_header_t;
// assert the struct size at compile time
typedef char static_assert_lcs_trace_header_size[(sizeof(struct lcs_trace_header) == 1024 * 8) ? 1 : -1];

/******************************************************************************/
/**       v1 is the simplest trace format (same as oracleGeneral)            **/
/** it only contains the clock time, obj_id, obj_size, and next_access_vtime **/
/**                                                                          **/
/** the next_access_vtime is the logical timestamp of next request           **/
/** in other words, is is the next_access_vtime th request in the trace      **/
/** if this is the last request in the trace, then it is -1                  **/
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
typedef struct __attribute__((packed)) lcs_req_v2 {
  uint32_t clock_time;
  uint64_t obj_id;
  uint32_t obj_size;
  uint32_t op : 8;
  uint32_t tenant : 24;
  int64_t next_access_vtime;
} lcs_req_v2_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v2_size[(sizeof(struct lcs_req_v2) == 28) ? 1 : -1];

/******************************************************************************/
/**              v3 uses int64_t for object size and adds ttl                **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v3 {
  uint32_t clock_time;
  uint64_t obj_id;
  int64_t obj_size;
  uint32_t op : 8;
  uint32_t tenant : 24;
  uint32_t ttl;
  int64_t next_access_vtime;
} lcs_req_v3_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v3_size[(sizeof(struct lcs_req_v3) == 36) ? 1 : -1];


/******************************************************************************/
/**              v4 has one feature field                                    **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v4 {
  lcs_req_v3_t base;
  uint32_t feature;
} lcs_req_v4_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v4_size[(sizeof(struct lcs_req_v4) == 40) ? 1 : -1];

/******************************************************************************/
/**              v5 has two feature fields                                   **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v5 {
  lcs_req_v3_t base;
  uint32_t features[2];
} lcs_req_v5_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v5_size[(sizeof(struct lcs_req_v5) == 44) ? 1 : -1];

/******************************************************************************/
/**              v6 has four feature fields                                  **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v6 {
  lcs_req_v3_t base;
  uint32_t features[4];
} lcs_req_v6_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v6_size[(sizeof(struct lcs_req_v6) == 52) ? 1 : -1];

/******************************************************************************/
/**              v7 has eight feature fields                                 **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v7 {
  lcs_req_v3_t base;
  uint32_t features[8];
} lcs_req_v7_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v7_size[(sizeof(struct lcs_req_v7) == 68) ? 1 : -1];

/******************************************************************************/
/**              v8 has sixteen feature fields                               **/
/******************************************************************************/
typedef struct __attribute__((packed)) lcs_req_v8 {
  lcs_req_v3_t base;
  uint32_t features[16];
} lcs_req_v8_t;
// assert the struct size at compile time
typedef char static_assert_lcs_v8_size[(sizeof(struct lcs_req_v8) == 100) ? 1 : -1];

static int LCS_VER_TO_N_FEATURES[10] = {0, 0, 0, 0, 1, 2, 4, 8, 16, 0};

int lcsReader_setup(reader_t *reader);

int lcs_read_one_req(reader_t *reader, request_t *req);

void lcs_print_trace_stat(reader_t *reader);

#ifdef __cplusplus
}
#endif

#pragma once

// LCSLLM trace format: a minimal binary trace that carries only
// clock_time, obj_id, cost, and next_access_vtime.
// obj_size defaults to 1; op, ttl, and tenant_id are not stored.

#include <inttypes.h>

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) lcsllm_req {
  uint32_t clock_time;        // 4 bytes
  uint64_t obj_id;            // 8 bytes
  int32_t cost;               // 4 bytes
  int64_t next_access_vtime;  // 8 bytes
} lcsllm_req_t;               // 24 bytes total

typedef char static_assert_lcsllm_req_size[(sizeof(lcsllm_req_t) == 24) ? 1 : -1];

static inline int lcsllmReader_setup(reader_t *reader) {
  reader->trace_type = LCSLLM_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = sizeof(lcsllm_req_t);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int lcsllm_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader, reader->item_size);
  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  lcsllm_req_t *r = (lcsllm_req_t *)record;
  req->clock_time = r->clock_time;
  req->obj_id = r->obj_id;
  req->cost = r->cost;
  req->next_access_vtime = r->next_access_vtime;
  req->obj_size = 1;

  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    req->next_access_vtime = MAX_REUSE_DISTANCE;
  }

  return 0;
}

#ifdef __cplusplus
}
#endif

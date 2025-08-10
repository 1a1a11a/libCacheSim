//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef libCacheSim_REQUEST_H
#define libCacheSim_REQUEST_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "enum.h"
#include "logging.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define N_MAX_FEATURES 16

/* need to optimize this for CPU cacheline */
typedef struct request {
  int64_t clock_time; /* use uint64_t because vscsi uses microsec timestamp */

  uint64_t hv; /* hash value, used when offloading hash to reader */

  /* this represents the hash of the object id in key-value cache
   * or the logical block address in block cache, note that LBA % block_size ==
   * 0 */
  obj_id_t obj_id;

  int64_t obj_size;

  int32_t ttl;

  req_op_e op;

  int32_t tenant_id;

  uint64_t n_req;

  int64_t next_access_vtime;

  // this is used by key-value cache traces
  struct {
    uint64_t key_size : 16;
    uint64_t val_size : 48;
  } kv;

  int32_t ns;  // namespace

  // carry necessary data between the multiple functions of serving one request
  void *eviction_algo_data;

  /* used in trace analysis */
  int64_t vtime_since_last_access;
  int64_t rtime_since_last_access;
  int64_t prev_size; /* prev size */
  int32_t create_rtime;
  bool compulsory_miss;      /* use this field only when it is set */
  bool overwrite;            // this request overwrites a previous object
  bool first_seen_in_window; /* the first time see in the time window */
  /* used in trace analysis */

  bool valid; /* indicate whether request is valid request
               * it is invalid if the trace reaches the end */

  int32_t n_features;
  int32_t features[N_MAX_FEATURES];

} request_t;

/**
 * allocate a new request_t struct and fill in necessary field
 * @return
 */
static inline request_t *new_request(void) {
  request_t *req = my_malloc(request_t);
  memset(req, 0, sizeof(request_t));
  req->obj_size = 1;
  req->op = OP_NOP;
  req->valid = true;
  req->obj_id = 0;
  req->clock_time = 0;
  req->hv = 0;
  req->next_access_vtime = -2;
  req->ttl = 0;
  return req;
}

/**
 * copy the req_src to req_dest
 * @param req_dest
 * @param req_src
 */
static inline void copy_request(request_t *req_dest, const request_t *req_src) {
  memcpy(req_dest, req_src, sizeof(request_t));
}

/**
 * clone the given request
 * @param req
 * @return
 */
static inline request_t *clone_request(const request_t *req) {
  request_t *req_new = my_malloc(request_t);
  copy_request(req_new, req);
  return req_new;
}

/**
 * free the memory used by req
 * @param req
 */
static inline void free_request(request_t *req) { my_free(request_t, req); }

static inline void print_request(const request_t *req) {
#ifdef SUPPORT_TTL
  LOGGING(DEBUG_LEVEL,
          "req clock_time %lu, id %llu, size %ld, ttl %ld, op %s, valid %d\n",
          (unsigned long)req->clock_time, (unsigned long long)req->obj_id,
          (long)req->obj_size, (long)req->ttl, req_op_str[req->op], req->valid);
#else
  LOGGING(DEBUG_LEVEL,
          "req clock_time %lu, id %llu, size %ld, op %s, valid %d\n",
          (unsigned long)req->clock_time, (unsigned long long)req->obj_id,
          (long)req->obj_size, req_op_str[req->op], req->valid);
#endif
}

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_REQUEST_H

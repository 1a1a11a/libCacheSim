//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef libCacheSim_REQUEST_H
#define libCacheSim_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging.h"
#include "mem.h"
#include "struct.h"
#include <stdio.h>
#include <string.h>

/**
 * allocate a new request_t struct and fill in necessary field
 * @return
 */
static inline request_t *new_request() {
  request_t *req = my_malloc(request_t);
  memset(req, 0, sizeof(request_t));
  req->obj_size = 1;
  req->op = OP_INVALID;
  req->valid = true;
  req->obj_id_int = 0;
  req->real_time = 0;
  req->hv = 0;
  return req;
}

/**
 * copy the req_src to req_dest
 * @param req_dest
 * @param req_src
 */
static inline void copy_request(request_t *req_dest, request_t *req_src) {
  memcpy(req_dest, req_src, sizeof(request_t));
}

/**
 * copy the cache_obj to req_dest
 * @param req_dest
 * @param cache_obj
 */
static inline void copy_cache_obj_to_request(request_t *req_dest,
    cache_obj_t *cache_obj) {
  req_dest->obj_id_int = cache_obj->obj_id_int;
  req_dest->obj_size = cache_obj->obj_size;
  req_dest->valid = true;
}


/**
 * clone the given request
 * @param req
 * @return
 */
static inline request_t *clone_request(request_t *req) {
  request_t *req_new = my_malloc(request_t);
  copy_request(req_new, req);
  return req_new;
}

/**
 * free the memory used by req
 * @param req
 */
static inline void free_request(request_t *req) {
  my_free(request_t, req);
}

static inline void print_request(request_t *req) {
#ifdef SUPPORT_TTL
  INFO("req real_time %lu, id %llu, size %ld, ttl %ld, op %s, valid %d\n",
       (unsigned long) req->real_time, (unsigned long long) req->obj_id_int,
       (long) req->obj_size, (long) req->ttl, OP_STR[req->op], req->valid);
#else
  printf("req real_time %lu, id %llu, size %ld, op %s, valid %d\n",
         (unsigned long)req->real_time, (unsigned long long)req->obj_id_int,
         (long)req->obj_size, OP_STR[req->op], req->valid);
#endif
}

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_REQUEST_H

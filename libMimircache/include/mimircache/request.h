//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_REQUEST_H
#define MIMIRCACHE_REQUEST_H

#ifdef __cplusplus
extern "C"
{
#endif

//#include <glib.h>
//#include <stdint.h>
//#include <string.h>
//#include "const.h"
//#include "enum.h"
//#include "../config.h"
#include "mem.h"
#include "struct.h"


static inline request_t *new_request() {
  request_t *req = my_malloc(request_t);
  memset(req, 0, sizeof(request_t));
  req->obj_size = 1;
  req->op = -1;
  req->valid = true;
  req->obj_id_int = 0;
  req->real_time = 0;
  return req;
}


static inline request_t *clone_request(request_t *req) {
  request_t *req_new = my_malloc(request_t);
  memcpy(req_new, req, sizeof(request_t));
  return req_new;
}

static inline void copy_request(request_t *req_dest, request_t *req_src) {
  memcpy(req_dest, req_src, sizeof(request_t));
}

static inline void free_request(request_t *req) {
  my_free(request_t, req);
}

static inline void print_request(request_t *req) {
  printf("req real_time %lu, id %llu, size %ld, ttl %ld, op %d, valid %d\n",
         (unsigned long) req->real_time, (unsigned long long) req->obj_id_int,
         (long) req->obj_size, (long) req->ttl, req->op, req->valid);
}


#ifdef __cplusplus
}
#endif

#endif //MIMIRCACHE_REQUEST_H

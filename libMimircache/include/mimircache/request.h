//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_REQUEST_H
#define MIMIRCACHE_REQUEST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>
#include <string.h>
#include "const.h"
#include "enum.h"
#include "../config.h"


/******************************* request ******************************/

/* need to optimize this for CPU cacheline */
typedef struct request {
  gint64 real_time;  // vscsi uses millisec timestamp
  /* pointer to obj_id if obj_id is str otherwise we use this as 64-bit int */
  gpointer obj_id_ptr;
  /* obj_id_type of content can be either size_t(OBJ_ID_NUM) or char*(OBJ_ID_STR)*/
  obj_id_type_t obj_id_type;

  gint32 obj_size;
  gint32 ttl;
  // used to indicate whether current request is a valid request and end of trace
  gint32 op;

  gboolean valid;
  /* leave one byte field at the end to reduce padding */


  // not used
//  gint64 extra_data;
//  void *extra_data_ptr;

  /* id of cache server, used in akamaiSimulator */
//  unsigned long cache_server_id;
//  unsigned char traceID; /* this is for mixed trace */

} request_t;


static inline request_t *new_request(obj_id_type_t obj_id_type) {
  request_t *req = g_new0(request_t, 1);
  req->obj_size = 1;
  req->op = -1;
  req->valid = TRUE;
  req->obj_id_type = obj_id_type;
  if (obj_id_type == OBJ_ID_STR)
    req->obj_id_ptr = g_new0(char, MAX_OBJ_ID_LEN);
  else
    req->obj_id_ptr = NULL;
  req->real_time = 0;
//  req->extra_data = 0;
//  req->extra_data_ptr = NULL;
  return req;
}


static inline request_t *clone_request(request_t *req) {
  request_t *req_new = g_new0(request_t, 1);
  // preserve the pointer before being overwritten by memcpy
  gpointer obj_id_ptr = req_new->obj_id_ptr;
  memcpy(req_new, req, sizeof(request_t));
  req_new->obj_id_ptr = obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    strcpy((char *) req_new->obj_id_ptr, (char *) req->obj_id_ptr);
  }
  return req_new;
}

static inline void copy_request(request_t *req_dest, request_t *req_src) {
  // preserve the pointer before being overwritten by memcpy
  gpointer obj_id_ptr = req_dest->obj_id_ptr;
  memcpy(req_dest, req_src, sizeof(request_t));
  req_dest->obj_id_ptr = obj_id_ptr;
  if (req_src->obj_id_type == OBJ_ID_STR) {
    strcpy((char *) req_dest->obj_id_ptr, (char *) req_src->obj_id_ptr);
  }
}

static inline void free_request(request_t *req) {
//  if (req->extra_data_ptr)
//    g_free(req->extra_data_ptr);
  if (req->obj_id_type == OBJ_ID_STR)
    g_free(req->obj_id_ptr);
  g_free(req);
}

static inline void print_request(request_t *req) {
  if (req->obj_id_type == OBJ_ID_NUM) {
    printf("req real_time %lu, id %llu, size %ld, ttl %ld, op %d, valid %d\n",
           (unsigned long) req->real_time, (unsigned long long) req->obj_id_ptr,
           (long) req->obj_size, (long) req->ttl, req->op, req->valid);
  } else if (req->obj_id_type == OBJ_ID_STR) {
    printf("req real_time %lu, id %s, size %ld, ttl %ld, op %d, valid %d\n",
           (unsigned long) req->real_time, (char *) req->obj_id_ptr,
           (long) req->obj_size, (long) req->ttl, req->op, req->valid);
  }
}


#ifdef __cplusplus
}
#endif

#endif //MIMIRCACHE_REQUEST_H

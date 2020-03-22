//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_REQUEST_H
#define MIMIRCACHE_REQUEST_H

#include <glib.h>
#include "const.h"


/******************************* request ******************************/

/* need to optimize this for CPU cacheline */
typedef struct request {
//  gint64 ts;
  gint64 real_time;
  /* pointer to obj_id */
  gpointer obj_id_ptr;
  /* obj_id_type of content can be either size_t(OBJ_ID_NUM) or char*(OBJ_ID_STR)*/
  obj_id_t obj_id_type;

  gint32 size;
  gint64 ttl;
  // used to check whether current request is a valid request
  gboolean valid;

  gint64 op;
  gint64 extra_data;
  void* extra_data_ptr;

  //  char obj_id[MAX_OBJ_ID_LEN];
//  size_t block_unit_size;
//  size_t disk_sector_size;

  // not used
  /* id of cache server, used in akamaiSimulator */
  unsigned long cache_server_id;
  unsigned char traceID; /* this is for mixed trace */

} request_t;


static inline request_t* new_request(obj_id_t obj_id_type){
  request_t* req = g_new0(request_t, 1);
  req->size = 1;
  req->op = -1;
  req->valid = TRUE;
  req->obj_id_type = obj_id_type;
  if (obj_id_type == OBJ_ID_STR)
    req->obj_id_ptr = g_new0(char, MAX_OBJ_ID_LEN);
  else
    req->obj_id_ptr = NULL;
  req->real_time = 0;
  req->extra_data = 0;
  req->extra_data_ptr = NULL;
  return req;
}


static inline request_t *clone_request(request_t *req){
  request_t* req_new = g_new0(request_t, 1);
  memcpy(req_new, req, sizeof(request_t));
  if (req->obj_id_type == OBJ_ID_STR){
    req_new->obj_id_ptr = g_new0(char, MAX_OBJ_ID_LEN);
    strcpy(req_new->obj_id_ptr, req->obj_id_ptr);
  }
  return req_new;
}


static inline void free_request(request_t* req){
  if (req->extra_data_ptr)
    g_free(req->extra_data_ptr);
  if (req->obj_id_type == OBJ_ID_STR)
    g_free(req->obj_id_ptr);
  g_free(req);
}



#endif //MIMIRCACHE_REQUEST_H

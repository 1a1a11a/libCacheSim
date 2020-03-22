//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_H
#define MIMIRCACHE_CACHEOBJ_H

#include "request.h"

typedef struct {
  // TODO(Jason): change this to a char array,
  //  so that reading obj requires one less indirection/memory access
  //  and it can be fit in CPU cache
  gpointer obj_id_ptr;
  guint32 size;
  gint64 extra_data;
  void *extra_data_ptr;
} cache_obj_t;


static inline cache_obj_t* create_cache_obj_from_req(request_t* req){
  cache_obj_t *cache_obj = g_new0(cache_obj_t, 1);
  cache_obj->size = req->size;
  cache_obj->extra_data = req->extra_data;
  cache_obj->extra_data_ptr = req->extra_data_ptr;
  cache_obj->obj_id_ptr = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  return cache_obj;
}

static inline void update_cache_obj(cache_obj_t* cache_obj, request_t* req){
  cache_obj->size = req->size;
  cache_obj->extra_data = req->extra_data;
  if (cache_obj->extra_data_ptr){
    g_free(cache_obj->extra_data_ptr);
  }
  cache_obj->extra_data_ptr = req->extra_data_ptr;
}

static inline void cache_obj_destroyer(gpointer data) {
  cache_obj_t *cache_obj = (cache_obj_t *) data;
  if (cache_obj->extra_data_ptr)
    g_free(cache_obj->extra_data_ptr);
  g_free(data);
}

static inline void destroy_cache_obj(gpointer data){
  cache_obj_destroyer(data);
}


#endif //MIMIRCACHE_CACHEOBJ_H

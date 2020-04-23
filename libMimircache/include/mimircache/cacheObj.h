//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_H
#define MIMIRCACHE_CACHEOBJ_H

#include "request.h"


typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
#ifdef SUPPORT_TTL
  gint32 ttl;
#endif
} cache_obj_t;


typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
  gint32 ttl;
  gchar extra_data[32];
} cache_obj_ext_t;

static inline cache_obj_t* create_cache_obj_from_req(request_t* req){
  cache_obj_t *cache_obj = g_new0(cache_obj_t, 1);
  cache_obj->obj_size = req->obj_size;
#ifdef SUPPORT_TTL
  cache_obj->ttl = req->ttl;
#endif
  cache_obj->obj_id_ptr = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  return cache_obj;
}

static inline void update_cache_obj(cache_obj_t* cache_obj, request_t* req){
  cache_obj->obj_size = req->obj_size;
#ifdef SUPPORT_TTL
  cache_obj->ttl = req->ttl;
#endif
}

static inline void cache_obj_destroyer(gpointer data) {
  g_free(data);
}

static inline void destroy_cache_obj(gpointer data){
  cache_obj_destroyer(data);
}


/**
 * slab based algorithm related
 */
typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
  gint32 ttl;
  void *slab;
  gint32 item_pos_in_slab;
} slab_cache_obj_t;

static inline slab_cache_obj_t* create_slab_cache_obj_from_req(request_t* req){
  slab_cache_obj_t *cache_obj = g_new0(slab_cache_obj_t, 1);
  cache_obj->obj_size = req->obj_size;
  cache_obj->ttl = req->real_time + req->ttl;
  cache_obj->obj_id_ptr = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  return cache_obj;
}







#endif //MIMIRCACHE_CACHEOBJ_H

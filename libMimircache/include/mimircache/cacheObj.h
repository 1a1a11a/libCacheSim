//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_H
#define MIMIRCACHE_CACHEOBJ_H

#include "../config.h"
#include "request.h"
#include "struct.h"
#include "mem.h"
#include <stdatomic.h>
#include <gmodule.h>


#ifdef USE_CUSTOME_MEM_ALLOCATOR
extern mem_allocator_t global_mem_alloc[N_MEM_ALLOCATOR];
extern gboolean global_mem_alloc_is_initialized;
#endif


static inline cache_obj_t* create_cache_obj_from_req(request_t* req){
#ifdef USE_CUSTOME_MEM_ALLOCATOR
  cache_obj_t *cache_obj = new_cache_obj();
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  cache_obj_t* cache_obj = g_slice_new(cache_obj_t);
#else
  cache_obj_t *cache_obj = g_new0(cache_obj_t, 1);
#endif

  cache_obj->obj_size = req->obj_size;
  if (req->ttl != 0)
    cache_obj->exp_time = req->real_time + req->ttl;
  cache_obj->obj_id_ptr = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  return cache_obj;
}


static inline void cache_obj_destroyer(gpointer data) {
#ifdef USE_CUSTOME_MEM_ALLOCATOR
  free_cache_obj((cache_obj_t*) data);
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  g_slice_free(cache_obj_t, data);
#else
  g_free(data);
#endif
}

static inline void cache_obj_destroyer_obj_id_str(gpointer data) {
  cache_obj_t *cache_obj = (cache_obj_t*) data;
  g_free(cache_obj->obj_id_ptr);
  cache_obj_destroyer(data);
}


static inline void destroy_cache_obj(gpointer data){
  cache_obj_destroyer(data);
}


//static inline void update_cache_obj(cache_obj_t *cache_obj, request_t *req) {
//  cache_obj->obj_size = req->obj_size;
//#ifdef SUPPORT_TTL
//  if (req->ttl > 0) // get request do not have TTL, so TTL is not reset
//    cache_obj->exp_time = req->real_time + req->ttl;
//#endif
//}


/**
 * slab based algorithm related
 */


static inline slab_cache_obj_t* create_slab_cache_obj_from_req(request_t* req){
#ifdef USE_CUSTOME_MEM_ALLOCATOR
  slab_cache_obj_t *cache_obj = new_cache_obj();
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  slab_cache_obj_t* cache_obj = g_slice_new(slab_cache_obj_t);
#else
  slab_cache_obj_t *cache_obj = g_new0(slab_cache_obj_t, 1);
#endif
  cache_obj->obj_size = req->obj_size;
  if (req->ttl != 0)
    cache_obj->exp_time = req->real_time + req->ttl;
  else
    cache_obj->exp_time = G_MAXINT32;
  cache_obj->obj_id_ptr = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  return cache_obj;
}


static inline void slab_cache_obj_destroyer(gpointer data){
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t*) data;

#ifdef USE_CUSTOME_MEM_ALLOCATOR
  free_cache_obj(cache_obj);
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  g_slice_free(slab_cache_obj_t, cache_obj);
#else
  g_free(data);
#endif
}


#endif //MIMIRCACHE_CACHEOBJ_H

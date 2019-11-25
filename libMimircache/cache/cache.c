//
//  cache.c
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


void cache_destroy(cache_t *cache) {
  if (cache->cache_params) {
    g_free(cache->cache_params);
    cache->cache_params = NULL;
  }

  /* cache->core->cache_init_params is on the stack default,
   if it is on the heap, needs to be freed manually
   */
//    if (cache->core->cache_init_params){
//        g_free(cache->core->cache_init_params);
//        cache->core->cache_init_params = NULL;
//    }

  g_free(cache->core);
  cache->core = NULL;
  g_free(cache);
}

void cache_destroy_unique(cache_t *cache) {
  if (cache->cache_params) {
    g_free(cache->cache_params);
    cache->cache_params = NULL;
  }
  g_free(cache->core);
  g_free(cache);
}


cache_t *cache_init(char *cache_name, long long cache_size, obj_id_t obj_id_type) {
  cache_t *cache = g_new0(cache_t, 1);
  strcpy(cache->core->cache_name, cache_name);
  cache->core = g_new0(struct cache_core, 1);
  cache->core->size = cache_size;
  cache->core->cache_init_params = NULL;
  cache->core->obj_id_type = obj_id_type;


  cache_t* (*cache_init)(guint64, obj_id_t, void*) = _get_cache_func_ptr(cache_name, "init");
  void (*destroy)(cache_t*) = _get_cache_func_ptr(cache_name, "destroy");
  void (*destroy_unique)(cache_t*) = _get_cache_func_ptr(cache_name, "destroy_unique");
  gboolean (*add)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "add");
  gboolean (*check)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "check");
  void *(*get_cached_data)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "get_cached_data");
  void (*update_cached_data)(cache_t*, request_t*, void *) = _get_cache_func_ptr(cache_name, "update_cached_data");
  void (*_insert)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "_insert");
  void (*_update)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "_update");
  void (*_evict)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "_evict");
  gpointer (*evict_with_return)(cache_t*, request_t*) = _get_cache_func_ptr(cache_name, "evict_with_return");
  guint64 (*get_current_size)(cache_t*) = _get_cache_func_ptr(cache_name, "get_current_size");
  GHashTable *(*get_objmap)(cache_t*) = _get_cache_func_ptr(cache_name, "get_objmap");
  void (*remove_obj)(cache_t*, void *) = _get_cache_func_ptr(cache_name, "remove_obj");

  cache->core->cache_init = cache_init;
  cache->core->destroy = destroy;
  cache->core->destroy_unique = destroy_unique;
  cache->core->add = add;
  cache->core->check = check;
  cache->core->_insert = _insert;
  cache->core->_update = _update;
  cache->core->_evict = _evict;
  cache->core->evict_with_return = evict_with_return;
  cache->core->get_current_size = get_current_size;
  cache->core->get_objmap = get_objmap;
  cache->core->remove_obj = remove_obj;
  cache->core->get_cached_data = get_cached_data;
  cache->core->update_cached_data = update_cached_data;


  return cache;
}


#ifdef __cplusplus
}
#endif

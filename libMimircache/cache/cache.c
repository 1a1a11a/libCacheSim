//
//  cache.c
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#include "../include/mimircache/cache.h"
#include "../include/mimircache/cacheOp.h"


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
  cache->core = g_new0(struct cache_core, 1);
  strcpy(cache->core->cache_name, cache_name);
  cache->core->size = cache_size;
  cache->core->cache_init_params = NULL;
  cache->core->obj_id_type = obj_id_type;

  void *handle = dlopen(NULL, RTLD_GLOBAL);
  char func_name[128];


  sprintf(func_name, "%s_init", cache_name);
  cache_t* (*cache_init)(guint64, obj_id_t, void*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_destroy", cache_name);
  void (*destroy)(cache_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_destroy_unique", cache_name);
  void (*destroy_unique)(cache_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_add", cache_name);
  gboolean (*add)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_check", cache_name);
  gboolean (*check)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_get_cached_data", cache_name);
  void *(*get_cached_data)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_update_cached_data", cache_name);
  void (*update_cached_data)(cache_t*, request_t*, void *) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_insert", cache_name);
  void (*_insert)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_update", cache_name);
  void (*_update)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_evict", cache_name);
  void (*_evict)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_evict_with_return", cache_name);
  gpointer (*evict_with_return)(cache_t*, request_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_get_current_size", cache_name);
  guint64 (*get_current_size)(cache_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_get_objmap", cache_name);
  GHashTable *(*get_objmap)(cache_t*) = dlsym(handle, func_name);

  sprintf(func_name, "%s_remove_obj", cache_name);
  void (*remove_obj)(cache_t*, void *) = dlsym(handle, func_name);


  if (cache_init == NULL)
    WARNING("cannot find cache_init for %s\n", cache_name);
  if (destroy == NULL)
    WARNING("cannot find function destroy for %s\n", cache_name);
  if (add == NULL)
    WARNING("cannot find function add for %s\n", cache_name);
  if (check == NULL)
    WARNING("cannot find function check for %s\n", cache_name);
  if (_insert == NULL)
    WARNING("cannot find function _insert for %s\n", cache_name);
  if (_update == NULL)
    WARNING("cannot find function _update for %s\n", cache_name);
  if (_evict == NULL)
    WARNING("cannot find function _evict for %s\n", cache_name);
  if (get_current_size == NULL)
    WARNING("cannot find function get_current_size for %s\n", cache_name);


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

//
//  cache.c
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//



#ifdef __cplusplus
extern "C"
{
#endif


#include "../include/mimircache/cache.h"


void cache_struct_free(cache_t *cache) {
  if (cache->cache_params) {
    g_free(cache->cache_params);
    cache->cache_params = NULL;
  }
//  g_free(cache->core);
//  cache->core = NULL;
  g_free(cache);
}


cache_t *cache_struct_init(const char* const cache_name, common_cache_params_t params) {
  cache_t *cache = g_new0(cache_t, 1);
  strncpy(cache->core.cache_name, cache_name, 32);
  cache->core.size = params.cache_size;
  cache->core.cache_specific_init_params = NULL;
  cache->core.obj_id_type = params.obj_id_type;
  cache->core.support_ttl = params.support_ttl;

  void *handle = dlopen(NULL, RTLD_GLOBAL);
  char func_name[128];


  sprintf(func_name, "%s_init", cache_name);
  cache_t *(*cache_init)(common_cache_params_t, void *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_free", cache_name);
  void (*cache_free)(cache_t *) = dlsym(handle, func_name);

//  sprintf(func_name, "%s_destroy_cloned_cache", cache_name);
//  void (*destroy_cloned_cache)(cache_t *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_get", cache_name);
  gboolean (*get)(cache_t *, request_t *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_check", cache_name);
  gboolean (*check)(cache_t *, request_t *) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_insert", cache_name);
  void (*_insert)(cache_t *, request_t *) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_update", cache_name);
  void (*_update)(cache_t *, request_t *) = dlsym(handle, func_name);

  sprintf(func_name, "_%s_evict", cache_name);
  void (*_evict)(cache_t *, request_t *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_evict_with_return", cache_name);
  gpointer (*evict_with_return)(cache_t *, request_t *) = dlsym(handle, func_name);


  /* non essential functions, can be NULL */
  sprintf(func_name, "%s_get_cached_obj", cache_name);
  cache_obj_t *(*get_cached_obj)(cache_t *, request_t *) = dlsym(handle, func_name);

//  sprintf(func_name, "%s_update_cached_data", cache_name);
//  void (*update_cached_data)(cache_t *, request_t *, void *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_get_objmap", cache_name);
  GHashTable *(*get_objmap)(cache_t *) = dlsym(handle, func_name);

  sprintf(func_name, "%s_remove_obj", cache_name);
  void (*remove_obj)(cache_t *, void *) = dlsym(handle, func_name);


  if (cache_init == NULL) {
    ERROR("cannot find cache_init for %s\n", cache_name);
    abort();
  }
  if (cache_free == NULL) {
    ERROR("cannot find function cache_free for %s\n", cache_name);
    abort();
  }
  if (get == NULL) {
    ERROR("cannot find function get for %s\n", cache_name);
    abort();
  }
  if (check == NULL) {
    ERROR("cannot find function check for %s\n", cache_name);
    abort();
  }
  if (_insert == NULL) {
    ERROR("cannot find function _insert for %s\n", cache_name);
    abort();
  }
  if (_update == NULL) {
    ERROR("cannot find function _update for %s\n", cache_name);
    abort();
  }
  if (_evict == NULL) {
    ERROR("cannot find function _evict for %s\n", cache_name);
    abort();
  }

  cache->core.cache_init = cache_init;
  cache->core.cache_free = cache_free;
  cache->core.get = get;
  cache->core.check = check;
  cache->core._insert = _insert;
  cache->core._update = _update;
  cache->core._evict = _evict;
  cache->core.evict_with_return = evict_with_return;
  cache->core.get_objmap = get_objmap;
  cache->core.remove_obj = remove_obj;
  cache->core.get_cached_obj = get_cached_obj;
  return cache;
}

cache_t *create_cache_with_new_size(cache_t *old_cache, gint64 new_size) {
  common_cache_params_t cc_params = {.cache_size=new_size,
      .obj_id_type=old_cache->core.obj_id_type,
      .support_ttl=old_cache->core.support_ttl};
  cache_t *cache = old_cache->core.cache_init(cc_params, old_cache->core.cache_specific_init_params);
  return cache;
}


#ifdef __cplusplus
}
#endif

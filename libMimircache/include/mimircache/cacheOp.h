//
// Created by Juncheng Yang on 3/20/20.
//

#ifndef LIBMIMIRCACHE_CACHEOP_H
#define LIBMIMIRCACHE_CACHEOP_H


#ifdef __cplusplus
extern "C" {
#endif


#include <assert.h>
#include "cache.h"
#include "profilerStruct.h"
#include "profiler.h"


/* cache_destroy and cache_destroy_cloned_cache are for cache users,
 * while cache_struct_init and cache_struct_free, cloned_cache_struct_destroy are for eviction alg writer,
 * likewise, create_cache is for cacher users, cache_init is for eviction algorithm writer */
/*
static inline void cache_destroy(cache_t *cache) { cache->core->destroy(cache); }

//static inline void cache_destroy_cloned_cache(cache_t *cache) { cache->core->destroy_cloned_cache(cache); }

static inline gboolean cache_get(cache_t *cache, request_t *req) { return cache->core->get(cache, req); }

static inline gboolean cache_check(cache_t *cache, request_t *req) { return cache->core->check(cache, req); }

static inline void *cache_get_cached_obj(cache_t *cache, request_t *req) {
  return cache->core->get_cached_obj(cache, req);
}

//static inline void cache_update_data(cache_t *cache, request_t *req, void *data) {
//  return cache->core->update_cached_data(cache, req, data);
//}

static inline void cache_insert(cache_t *cache, request_t *req) { return cache->core->_insert(cache, req); }

static inline void cache_update(cache_t *cache, request_t *req) { return cache->core->_update(cache, req); }

static inline void cache_evict(cache_t *cache, request_t *req) { cache->core->_evict(cache, req); }

static inline void *cache_evict_with_return(cache_t *cache, request_t *req) {
  return cache->core->evict_with_return(cache, req);
}

static inline guint64 cache_get_used_size(cache_t *cache) { return cache->core->used_size; }

static inline GHashTable *cache_get_obj_map(cache_t *cache) { return cache->core->get_objmap(cache); }

static inline void cache_remove(cache_t *cache, void *data) { cache->core->remove_obj(cache, data); }
*/

static inline void *_get_cache_func_ptr(char *func_name) {
  void *handle = dlopen(NULL, RTLD_GLOBAL);
  void *func_ptr = dlsym(handle, func_name);
  return func_ptr;

//  char *error;
//  if ((error = dlerror()) != NULL) {
//    WARNING("error loading %s %s\n", full_func_name, error);
//    return NULL;
//  } else {
//    DEBUG("internal cache loaded %s\n", full_func_name);
//    return func_ptr;
//  }
}


profiler_res_t *run_trace(cache_t *cache, reader_t *reader);

static inline profiler_res_t *warmup(cache_t *cache, reader_t *reader) {
  return run_trace(cache, reader);
}

static inline profiler_res_t *eval(cache_t *cache, reader_t *reader) {
  return run_trace(cache, reader);
}


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CACHEOP_H

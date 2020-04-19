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
//#include "../include/mimircache/cacheOp.h"



void cache_struct_free(cache_t *cache) {
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


cache_t *cache_struct_init(char *cache_name, long long cache_size, obj_id_type_t obj_id_type) {
  cache_t *cache = g_new0(cache_t, 1);
  cache->core = g_new0(struct cache_core, 1);
  strcpy(cache->core->cache_name, cache_name);
  cache->core->size = cache_size;
  cache->core->cache_init_params = NULL;
  cache->core->obj_id_type = obj_id_type;

  void *handle = dlopen(NULL, RTLD_GLOBAL);
  char func_name[128];


  sprintf(func_name, "%s_init", cache_name);
  cache_t *(*cache_init)(guint64, obj_id_type_t, void *) = dlsym(handle, func_name);

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
  cache_obj_t* (*get_cached_obj)(cache_t *, request_t *) = dlsym(handle, func_name);

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

  cache->core->cache_init = cache_init;
  cache->core->cache_free = cache_free;
  cache->core->get = get;
  cache->core->check = check;
  cache->core->_insert = _insert;
  cache->core->_update = _update;
  cache->core->_evict = _evict;
  cache->core->evict_with_return = evict_with_return;
  cache->core->get_objmap = get_objmap;
  cache->core->remove_obj = remove_obj;
  cache->core->get_cached_obj = get_cached_obj;
  return cache;
}

//profiler_res_t *run_trace(cache_t *cache, reader_t *reader) {
//  request_t *req = new_request(cache->core->obj_id_type);
//  profiler_res_t *ret = g_new0(profiler_res_t, 1);
//  ret->cache_size = cache->core->size;
//  gboolean (*cache_get)(cache_t *, request_t *) = cache->core->get;
//
//  read_one_req(reader, req);
//  while (req->valid) {
//    if (!cache_get(cache, req)) {
//      ret->miss_cnt += 1;
//      ret->miss_byte += req->size;
//    }
//    ret->req_cnt += 1;
//    ret->req_byte += req->size;
//
//    read_one_req(reader, req);
//  }
//  ret->byte_miss_ratio = (double) ret->miss_byte / (double) ret->req_byte;
//  ret->obj_miss_ratio = (double) ret->miss_cnt / (double) ret->req_cnt;
//
//  // clean up
//  free_request(req);
//  reset_reader(reader);
//  return ret;
//}


#ifdef __cplusplus
}
#endif

//
//  a lazyFIFO module that supports different obj size
//
//
//  lazyFIFO.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_ck_res_e lazyFIFO_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_ck_res_e lazyFIFO_check(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  cache_obj_t *cached_obj = NULL;
  cache_ck_res_e ck = cache_check_base(cache, req, update_cache, &cached_obj);
  if (cached_obj != NULL) {
    cached_obj->misc.freq += 1;
    cached_obj->misc.last_access_rtime = req->real_time;
    cached_obj->misc.last_access_vtime = cache->n_req;
    cached_obj->misc.next_access_vtime = req->next_access_vtime;
  }

  return ck;
}

cache_obj_t *lazyFIFO_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_LRU(cache, req);
  obj->misc.freq = 1;
  obj->misc.last_access_rtime = req->real_time;
  obj->misc.last_access_vtime = cache->n_req;
  obj->misc.next_access_vtime = req->next_access_vtime;

  return obj;
}

cache_obj_t *lazyFIFO_to_evict(cache_t *cache) {
  cache_obj_t *obj_to_evict = cache->q_tail;
  while (obj_to_evict->misc.freq > 1) {
    obj_to_evict->misc.freq = 1;
    move_obj_to_head(&cache->q_head, &cache->q_tail, obj_to_evict);
    obj_to_evict = cache->q_tail;
  }

  return obj_to_evict;
}

void lazyFIFO_evict(cache_t *cache, const request_t *req,
                    cache_obj_t *evicted_obj) {
  cache_obj_t *obj_to_evict = lazyFIFO_to_evict(cache);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_evict);
  cache_remove_obj_base(cache, obj_to_evict);
}

void lazyFIFO_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  if (obj_to_remove == NULL) {
    ERROR("remove NULL from cache\n");
    abort();
  }

  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove);
}

void lazyFIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached\n", obj_id);
    return;
  }
  lazyFIFO_remove_obj(cache, obj);
}

void lazyFIFO_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *lazyFIFO_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("lazyFIFO", ccache_params);
  cache->cache_init = lazyFIFO_init;
  cache->cache_free = lazyFIFO_free;
  cache->get = lazyFIFO_get;
  cache->check = lazyFIFO_check;
  cache->insert = lazyFIFO_insert;
  cache->evict = lazyFIFO_evict;
  cache->remove = lazyFIFO_remove;
  cache->to_evict = lazyFIFO_to_evict;
  cache->init_params = cache_specific_params;
  cache->per_obj_metadata_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  return cache;
}

#ifdef __cplusplus
}
#endif

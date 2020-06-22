//
//  a FIFO module that supports different obj size
//
//
//  FIFO.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/evictionAlgo/FIFO.h"
#include "include/cacheAlg.h"
#include <assert.h>

cache_t *FIFO_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("FIFO", ccache_params);
  return cache;
}

void FIFO_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e FIFO_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check(cache, req, update_cache, NULL);
}

cache_ck_res_e FIFO_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void FIFO_insert(cache_t *FIFO, request_t *req) {
  cache_insert_LRU(FIFO, req);
}

void FIFO_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  // currently not handle the case when all objects are evicted
  cache_obj_t *obj_to_evict = cache->list_head;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
  cache->list_head = cache->list_head->list_next;
  cache->list_head->list_prev = NULL;
  DEBUG_ASSERT(cache->occupied_size >= obj_to_evict->obj_size);
  cache->occupied_size -= obj_to_evict->obj_size;
  hashtable_delete(cache->hashtable, obj_to_evict);
  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
  /** obj_to_evict is not freed or returned to hashtable, if you have
   * extra_metadata allocated with obj_to_evict, you need to free them now,
   * otherwise, there will be memory leakage **/
}

void FIFO_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail,
                       cache_obj);
  hashtable_delete(cache->hashtable, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;
}

#ifdef __cplusplus
}
#endif

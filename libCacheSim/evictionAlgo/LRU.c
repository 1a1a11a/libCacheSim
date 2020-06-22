//
//  a LRU module that supports different obj size
//
//
//  LRU.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/evictionAlgo/LRU.h"
#include "include/cacheAlg.h"

cache_t *LRU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LRU", ccache_params);
  return cache;
}

void LRU_free(cache_t *cache) {
  cache_struct_free(cache);
}

cache_ck_res_e LRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->list_head, &cache->list_tail, cache_obj);
  }
  return ret;
}

cache_ck_res_e LRU_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void LRU_insert(cache_t *LRU, request_t *req) {
  cache_insert_LRU(LRU, req);
}

void LRU_evict(cache_t *LRU, request_t *req, cache_obj_t *cache_obj) {
  // currently not handle the case when all objects are evicted
  cache_obj_t *obj_to_evict = LRU->list_head;
  if (cache_obj != NULL) {
    // return evicted object to caller
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(LRU->list_head != LRU->list_head->list_next);
  LRU->list_head = LRU->list_head->list_next;
  LRU->list_head->list_prev = NULL;
  DEBUG_ASSERT(LRU->occupied_size >= obj_to_evict->obj_size);
  LRU->occupied_size -= obj_to_evict->obj_size;
  hashtable_delete(LRU->hashtable, obj_to_evict);
  //  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
  /** obj_to_evict is not freed or returned to hashtable, if you have
   * extra_metadata allocated with obj_to_evict, you need to free them now,
   * otherwise, there will be memory leakage **/
}

void LRU_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);
  hashtable_delete(cache->hashtable, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;
}

#ifdef __cplusplus
extern "C" {
#endif

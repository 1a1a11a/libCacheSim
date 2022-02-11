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

#include "../include/libCacheSim/evictionAlgo/LRU.h"
#include "../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif


cache_t *LRU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LRU", ccache_params);
  cache->cache_init = LRU_init;
  cache->cache_free = LRU_free;
  cache->get = LRU_get;
  cache->check = LRU_check;
  cache->insert = LRU_insert;
  cache->evict = LRU_evict;
  cache->remove = LRU_remove;
  cache->to_evict = LRU_to_evict;
  return cache;
}

void LRU_free(cache_t *cache) {
  cache_struct_free(cache);
}

cache_ck_res_e LRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->q_head, &cache->q_tail, cache_obj);
  }
  return ret;
}

cache_ck_res_e LRU_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void LRU_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

cache_obj_t *LRU_to_evict(cache_t *cache){
  return cache->q_head;
}

void LRU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}

void LRU_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    WARN("obj (%"PRIu64 ") to remove is not in the cache\n", obj_id);
    return;
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj);
  cache_remove_obj_base(cache, obj);
}


#ifdef __cplusplus
extern "C" {
#endif

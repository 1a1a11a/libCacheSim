//
//  MRU.c
//  libCacheSim
//
//  MRU evictionAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifdef _cplusplus
extern "C" {
#endif

#include "include/cacheAlg.h"
#include "../include/libCacheSim/evictionAlgo/MRU.h"

cache_t *MRU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params);
  return cache;
}

void MRU_free(cache_t *cache) { cache_struct_free(cache); }

void MRU_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

cache_ck_res_e MRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->list_head, &cache->list_tail, cache_obj);
  }
  return ret;
}

void MRU_evict(cache_t *cache, request_t *req, cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict = cache->list_tail;
  if (cache_obj != NULL) {
    // return evicted object to caller
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  cache->list_tail = cache->list_tail->list_prev;
  cache->list_tail->list_next = NULL;
  DEBUG_ASSERT(cache->occupied_size >= obj_to_evict->obj_size);
  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);
  hashtable_delete(cache->hashtable, obj_to_evict);
}

cache_ck_res_e MRU_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

#ifdef _cplusplus
}
#endif

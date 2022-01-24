//
//  MRU.c
//  libCacheSim
//
//  MRU eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/MRU.h"

#ifdef _cplusplus
extern "C" {
#endif

cache_t *MRU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params);
  cache->cache_init = MRU_init;
  cache->cache_free = MRU_free;
  cache->get = MRU_get;
  cache->check = MRU_check;
  cache->insert = MRU_insert;
  cache->evict = MRU_evict;
  cache->remove = MRU_remove;

  return cache;
}

void MRU_free(cache_t *cache) { cache_struct_free(cache); }

void MRU_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

cache_ck_res_e MRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->q_head, &cache->q_tail, cache_obj);
  }
  return ret;
}

void MRU_evict(cache_t *cache, request_t *req, cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict = cache->q_tail;
  if (cache_obj != NULL) {
    // return evicted object to caller
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  cache->q_tail = cache->q_tail->queue.prev;
  cache->q_tail->queue.next = NULL;
  cache_remove_obj_base(cache, obj_to_evict);
}

cache_ck_res_e MRU_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void MRU_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj);
  cache_remove_obj_base(cache, obj);
}

#ifdef _cplusplus
}
#endif

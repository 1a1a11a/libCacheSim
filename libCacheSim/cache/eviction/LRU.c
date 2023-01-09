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

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/LRU.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define USE_BELADY

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
cache_t *LRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRU", ccache_params);
  cache->cache_init = LRU_init;
  cache->cache_free = LRU_free;
  cache->get = LRU_get;
  cache->check = LRU_check;
  cache->insert = LRU_insert;
  cache->evict = LRU_evict;
  cache->remove = LRU_remove;
  cache->to_evict = LRU_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

#ifdef USE_BELADY
  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "LRU_Reinsertion_Belady");
#endif

  LRU_params_t *params = malloc(sizeof(LRU_params_t));
  params->q_head = NULL;
  params->q_tail = NULL;
  cache->eviction_params = params;

  return cache;
}

void LRU_free(cache_t *cache) { cache_struct_free(cache); }

bool LRU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
bool LRU_check(cache_t *cache, const request_t *req, const bool update_cache) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_head is the newest, move cur obj to lru_head */
#ifdef USE_BELADY
    if (req->next_access_vtime != INT64_MAX)
#endif
      move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  }
  return cache_hit;
}

cache_obj_t *LRU_insert(cache_t *cache, const request_t *req) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  return obj;
}

cache_obj_t *LRU_to_evict(cache_t *cache) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  return params->q_tail;
}

void LRU_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  // we can simply call remove_obj_from_list here, but for the best performance,
  // we chose to do it manually
  // remove_obj_from_list(&params->q_head, &params->q_tail, obj)

  params->q_tail = params->q_tail->queue.prev;
  if (likely(params->q_tail != NULL)) {
    params->q_tail->queue.next = NULL;
  } else {
    /* cache->n_obj has not been updated */
    DEBUG_ASSERT(cache->n_obj == 1);
    params->q_head = NULL;
  }

  cache_evict_base(cache, obj_to_evict, true);
}

bool LRU_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    return false;
  }
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

#ifdef __cplusplus
}
#endif

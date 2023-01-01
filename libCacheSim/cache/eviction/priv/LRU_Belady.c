//
//  a LRU_Belady module that supports different obj size
//
//
//  LRU_Belady.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../include/libCacheSim/evictionAlgo/FIFO.h"
#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/dist.h"
#include "../../include/libCacheSim/evictionAlgo/priv/LRU_Belady.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} LRU_Belady_params_t;

cache_t *LRU_Belady_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRU_Belady", ccache_params);
  cache->cache_init = LRU_Belady_init;
  cache->cache_free = LRU_Belady_free;
  cache->get = LRU_Belady_get;
  cache->check = LRU_Belady_check;
  cache->insert = LRU_Belady_insert;
  cache->evict = LRU_Belady_evict;
  cache->remove = LRU_Belady_remove;
  cache->to_evict = LRU_Belady_to_evict;
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

  return cache;
}

void LRU_Belady_free(cache_t *cache) { cache_struct_free(cache); }

bool LRU_Belady_check(cache_t *cache, const request_t *req,
                      const bool update_cache) {
  cache_obj_t *cache_obj;
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache->n_req > cache->future_stack_dist_array_size) {
    ERROR(
        "cache->n_req >= reader->n_req, cache->n_req=%lu, "
        "stack_dist_array_size=%lu\n",
        cache->n_req, cache->future_stack_dist_array_size);
  }

  if (cache_obj && likely(update_cache)) {
    if (cache->future_stack_dist[cache->n_req - 1] != -1 &&
        cache->future_stack_dist[cache->n_req - 1] < cache->cache_size) {
      move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
    }
  }
  return cache_hit;
}

bool LRU_Belady_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *LRU_Belady_insert(cache_t *cache, const request_t *req) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  return obj;
}

cache_obj_t *LRU_Belady_to_evict(cache_t *cache) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  return params->q_tail;
}

void LRU_Belady_evict(cache_t *cache, const request_t *req,
                      cache_obj_t *evicted_obj) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_evict_obj_base(cache, obj_to_evict);
}

bool LRU_Belady_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    return false;
  }

  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj);

  return true;
}

#ifdef __cplusplus
}
#endif

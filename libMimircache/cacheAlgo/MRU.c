//
//  MRU.c
//  libMimircache
//
//  MRU cacheAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifdef _cplusplus
extern "C" {
#endif


#include "MRU.h"


cache_t *MRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params);
  return cache;
}

void MRU_free(cache_t *cache) {
  cache_struct_free(cache);
}


void _MRU_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

cache_check_result_t MRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_check_result_t ret = cache_check(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->core.list_head, &cache->core.list_tail, cache_obj);
  }
  return ret;
}


void _MRU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_obj_t *obj_to_evict = cache->core.list_tail;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  cache->core.list_tail = cache->core.list_tail->list_prev;
  cache->core.list_tail->list_next = NULL;
  DEBUG_ASSERT(cache->core.used_size >= obj_to_evict->obj_size);
  cache->core.used_size -= obj_to_evict->obj_size;
  hashtable_delete(cache->core.hashtable_new, obj_to_evict);
}

cache_check_result_t MRU_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}



#ifdef _cplusplus
}
#endif

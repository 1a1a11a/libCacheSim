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

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/FIFO.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *FIFO_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("FIFO", ccache_params);
  cache->cache_init = FIFO_init;
  cache->cache_free = FIFO_free;
  cache->get = FIFO_get;
  cache->check = FIFO_check;
  cache->insert = FIFO_insert;
  cache->evict = FIFO_evict;
  cache->remove = FIFO_remove;
  cache->to_evict = FIFO_to_evict;
  cache->init_params = cache_specific_params;
  cache->per_obj_metadata_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  return cache;
}

void FIFO_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e FIFO_check(cache_t *cache, const request_t *req,
                          const bool update_cache) {
  return cache_check_base(cache, req, update_cache, NULL);
}

cache_ck_res_e FIFO_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *FIFO_insert(cache_t *cache, const request_t *req) {
  return cache_insert_LRU(cache, req);
}

cache_obj_t *FIFO_to_evict(cache_t *cache) { return cache->q_tail; }

void FIFO_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}

void FIFO_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  if (obj_to_remove == NULL) {
    ERROR("remove NULL from cache\n");
    abort();
  }

  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove);
}

void FIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached\n", obj_id);
    return;
  }
  FIFO_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

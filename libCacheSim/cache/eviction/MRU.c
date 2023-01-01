//
//  MRU.c
//  libCacheSim
//
//  MRU eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/MRU.h"

#ifdef _cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} MRU_params_t;

cache_t *MRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params);
  cache->cache_init = MRU_init;
  cache->cache_free = MRU_free;
  cache->get = MRU_get;
  cache->check = MRU_check;
  cache->insert = MRU_insert;
  cache->evict = MRU_evict;
  cache->to_evict = MRU_to_evict;
  cache->remove = MRU_remove;
  cache->init_params = cache_specific_params;

  if (cache_specific_params != NULL) {
    printf("MRU does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(MRU_params_t));
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

void MRU_free(cache_t *cache) { cache_struct_free(cache); }

cache_obj_t *MRU_insert(cache_t *cache, const request_t *req) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  return obj;
}

bool MRU_check(cache_t *cache, const request_t *req, const bool update_cache) {
  cache_obj_t *cache_obj;
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  }
  return cache_hit;
}

cache_obj_t *MRU_to_evict(cache_t *cache) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  return params->q_head;
}

void MRU_evict(cache_t *cache, const request_t *req, cache_obj_t *cache_obj) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = MRU_to_evict(cache);
  if (cache_obj != NULL) {
    // return evicted object to caller
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  params->q_head = params->q_head->queue.next;
  params->q_head->queue.prev = NULL;
  cache_evict_obj_base(cache, obj_to_evict);
}

bool MRU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

bool MRU_remove(cache_t *cache, const obj_id_t obj_id) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj);

  return true;
}

#ifdef _cplusplus
}
#endif

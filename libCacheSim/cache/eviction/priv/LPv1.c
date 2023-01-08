//
//  similar to fifo-reinsertion, but instead of using 1-bit to indicate whether
//  the object is requested after written, LPv1 tracks frequency, and each round
//  reduces frequency by 1
//
//
//  LPv1.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} LPv1_params_t;

bool LPv1_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

bool LPv1_check(cache_t *cache, const request_t *req, const bool update_cache) {
  cache_obj_t *cached_obj = NULL;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cached_obj);
  if (cached_obj != NULL) {
    cached_obj->misc.freq += 1;
    cached_obj->misc.last_access_rtime = req->clock_time;
    cached_obj->misc.last_access_vtime = cache->n_req;
    cached_obj->misc.next_access_vtime = req->next_access_vtime;
  }

  return cache_hit;
}

cache_obj_t *LPv1_insert(cache_t *cache, const request_t *req) {
  LPv1_params_t *params = (LPv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  obj->misc.freq = 1;
  obj->misc.last_access_rtime = req->clock_time;
  obj->misc.last_access_vtime = cache->n_req;
  obj->misc.next_access_vtime = req->next_access_vtime;

  return obj;
}

cache_obj_t *LPv1_to_evict(cache_t *cache) {
  LPv1_params_t *params = (LPv1_params_t *)cache->eviction_params;

  cache_obj_t *obj_to_evict = params->q_tail;
  while (obj_to_evict->misc.freq > 1) {
    obj_to_evict->misc.freq -= 1;
    move_obj_to_head(&params->q_head, &params->q_tail, obj_to_evict);
    obj_to_evict = params->q_tail;
  }

  return obj_to_evict;
}

void LPv1_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  LPv1_params_t *params = (LPv1_params_t *)cache->eviction_params;

  cache_obj_t *obj_to_evict = LPv1_to_evict(cache);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_evict_base(cache, obj_to_evict, true);
}

void LPv1_remove_obj(cache_t *cache, cache_obj_t *obj) {
  LPv1_params_t *params = (LPv1_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

bool LPv1_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  LPv1_remove_obj(cache, obj);

  return true;
}

void LPv1_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *LPv1_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LPv1", ccache_params);
  cache->cache_init = LPv1_init;
  cache->cache_free = LPv1_free;
  cache->get = LPv1_get;
  cache->check = LPv1_check;
  cache->insert = LPv1_insert;
  cache->evict = LPv1_evict;
  cache->remove = LPv1_remove;
  cache->to_evict = LPv1_to_evict;
  cache->init_params = cache_specific_params;
  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(LPv1_params_t));
  LPv1_params_t *params = (LPv1_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

#ifdef __cplusplus
}
#endif

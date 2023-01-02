//
//  lazy LRU - does not promote until eviction time
//  this is FIFO with re-insertion
//
//  it is different from both Clock and segmented FIFO with re-insertion
//  compare to Clock, old objects are mixed with new objects upon re-insertion
//  compare to segmented FIFO-Reinsertion, this can promote as many objects as
//  needed
//
//
//  FIFO_Reinsertion.c
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
} FIFO_Reinsertion_params_t;

bool FIFO_Reinsertion_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

bool FIFO_Reinsertion_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
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

cache_obj_t *FIFO_Reinsertion_insert(cache_t *cache, const request_t *req) {
  FIFO_Reinsertion_params_t *params =
      (FIFO_Reinsertion_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  obj->misc.freq = 1;
  obj->misc.last_access_rtime = req->clock_time;
  obj->misc.last_access_vtime = cache->n_req;
  obj->misc.next_access_vtime = req->next_access_vtime;

  return obj;
}

cache_obj_t *FIFO_Reinsertion_to_evict(cache_t *cache) {
  FIFO_Reinsertion_params_t *params =
      (FIFO_Reinsertion_params_t *)cache->eviction_params;

  cache_obj_t *obj_to_evict = params->q_tail;
  while (obj_to_evict->misc.freq > 1) {
    obj_to_evict->misc.freq = 1;
    move_obj_to_head(&params->q_head, &params->q_tail, obj_to_evict);
    obj_to_evict = params->q_tail;
  }

  return obj_to_evict;
}

void FIFO_Reinsertion_evict(cache_t *cache, const request_t *req,
                            cache_obj_t *evicted_obj) {
  FIFO_Reinsertion_params_t *params =
      (FIFO_Reinsertion_params_t *)cache->eviction_params;

  cache_obj_t *obj_to_evict = FIFO_Reinsertion_to_evict(cache);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_evict_base(cache, obj_to_evict);
}

void FIFO_Reinsertion_remove_obj(cache_t *cache, cache_obj_t *obj) {
  FIFO_Reinsertion_params_t *params =
      (FIFO_Reinsertion_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj);
}

bool FIFO_Reinsertion_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  FIFO_Reinsertion_remove_obj(cache, obj);

  return true;
}

void FIFO_Reinsertion_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *FIFO_Reinsertion_init(const common_cache_params_t ccache_params,
                               const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("FIFO_Reinsertion", ccache_params);
  cache->cache_init = FIFO_Reinsertion_init;
  cache->cache_free = FIFO_Reinsertion_free;
  cache->get = FIFO_Reinsertion_get;
  cache->check = FIFO_Reinsertion_check;
  cache->insert = FIFO_Reinsertion_insert;
  cache->evict = FIFO_Reinsertion_evict;
  cache->remove = FIFO_Reinsertion_remove;
  cache->to_evict = FIFO_Reinsertion_to_evict;
  cache->init_params = cache_specific_params;
  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(FIFO_Reinsertion_params_t));
  FIFO_Reinsertion_params_t *params =
      (FIFO_Reinsertion_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

#ifdef __cplusplus
}
#endif

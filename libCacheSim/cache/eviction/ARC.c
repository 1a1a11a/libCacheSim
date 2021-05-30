//
//  ARC cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//


#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/LRU.h"
#include "../include/libCacheSim/evictionAlgo/ARC.h"


#ifdef __cplusplus
extern "C" {
#endif

cache_t *ARC_init(common_cache_params_t ccache_params_, void *init_params_) {
  cache_t *cache = cache_struct_init("ARC", ccache_params_);
  cache->cache_init = ARC_init;
  cache->cache_free = ARC_free;
  cache->get = ARC_get;
  cache->check = ARC_check;
  cache->insert = ARC_insert;
  cache->evict = ARC_evict;
  cache->remove = ARC_remove;

  cache->init_params = init_params_;
  ARC_init_params_t *init_params = (ARC_init_params_t *) init_params_;

  cache->eviction_algo = my_malloc_n(ARC_params_t, 1);
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  if (init_params_ != NULL)
    ARC_params->ghost_list_factor = init_params->ghost_list_factor;
  else
    ARC_params->ghost_list_factor = 1;

  ARC_params->LRU1 = LRU_init(ccache_params_, NULL);
  ARC_params->LRU2 = LRU_init(ccache_params_, NULL);

  ccache_params_.cache_size /= 2;
  ccache_params_.cache_size *= ARC_params->ghost_list_factor;
  ARC_params->LRU1g = LRU_init(ccache_params_, NULL);
  ARC_params->LRU2g = LRU_init(ccache_params_, NULL);

  return cache;
}

void ARC_free(cache_t *cache) {
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  ARC_params->LRU1->cache_free(ARC_params->LRU1);
  ARC_params->LRU1g->cache_free(ARC_params->LRU1g);
  ARC_params->LRU2->cache_free(ARC_params->LRU2);
  ARC_params->LRU2g->cache_free(ARC_params->LRU2g);
  my_free(sizeof(ARC_params_t), ARC_params);
  cache_struct_free(cache);
}

cache_ck_res_e ARC_check(cache_t *cache, request_t *req, bool update_cache) {
//  static __thread cache_obj_t cache_obj;
  static __thread request_t *req_local = NULL;
  if (req_local == NULL)
    req_local = new_request();

  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  uint64_t old_sz;

  cache_ck_res_e hit1, hit2, hit1g, hit2g;
  hit1 = ARC_params->LRU1->check(ARC_params->LRU1, req, false);
  cache_ck_res_e hit = hit1;
  if (hit1 != cache_ck_hit) {
    hit1g = ARC_params->LRU1g->check(ARC_params->LRU1g, req, true & update_cache);
    if (hit1g) {
      /* hit on LRU1 ghost list */
      ARC_params->move_pos = 2;
    }
    old_sz = ARC_params->LRU2->occupied_size;
    hit2 = ARC_params->LRU2->check(ARC_params->LRU2, req, true & update_cache);
    cache->occupied_size += ARC_params->LRU2->occupied_size - old_sz;
    if (hit2 == cache_ck_hit) {
      hit = cache_ck_hit;
    } else {
      hit2g = ARC_params->LRU2g->check(ARC_params->LRU2g, req, true & update_cache);
      if (hit2g) {
        /* hit on LRU2 ghost list */
        ARC_params->move_pos = 1;
      }
    }
  }

  if (!update_cache)
    return hit;

  if (hit != cache_ck_hit) {
    return hit;
  }

  if (hit1 == cache_ck_hit) {
    /* because the object size may change, so we have to obtain the object size */
    old_sz = ARC_params->LRU1->occupied_size;
    ARC_params->LRU1->remove(ARC_params->LRU1, req->obj_id_int);
    cache->occupied_size -= old_sz - ARC_params->LRU1->occupied_size;

    ARC_params->LRU2->insert(ARC_params->LRU2, req);
    cache->occupied_size += req->obj_size;
  } else if (hit2 == cache_ck_hit) {
    /* moving to the tail of LRU2 has already been done */
  }
  return hit;
}

cache_ck_res_e ARC_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void ARC_insert(cache_t *cache, request_t *req) {
  /* first time add, then it should be add to LRU1 */
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  ARC_params->LRU1->insert(ARC_params->LRU1, req);

  cache->occupied_size += req->obj_size;
  DEBUG_ASSERT(cache->occupied_size ==
    ARC_params->LRU1->occupied_size + ARC_params->LRU2->occupied_size);
}


void ARC_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  static __thread cache_obj_t cache_obj;
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  cache_t *cache_evict, *cache_evict_ghost;

  if (ARC_params->move_pos == 1) {
    cache_evict = ARC_params->LRU1;
    cache_evict_ghost = ARC_params->LRU1g;
  } else {
    cache_evict = ARC_params->LRU2;
    cache_evict_ghost = ARC_params->LRU2g;
  }

  cache_evict->evict(cache_evict, req, &cache_obj);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &cache_obj, sizeof(cache_obj_t));
  }
  copy_cache_obj_to_request(req_local, &cache_obj);
  cache_evict_ghost->get(cache_evict_ghost, req_local);
  cache->occupied_size -= cache_obj.obj_size;
}

void ARC_remove(cache_t *cache, obj_id_t obj_id) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL)
    req_local = new_request();

  ARC_params_t *ARC_params = (ARC_params_t *) (cache->eviction_algo);
  cache_obj_t *obj = hashtable_find_obj_id(ARC_params->LRU1->hashtable, obj_id);
  if (obj != NULL) {
    copy_cache_obj_to_request(req_local, obj);
    ARC_params->LRU1->remove(ARC_params->LRU1, obj_id);
  } else {
    obj = hashtable_find_obj_id(ARC_params->LRU2->hashtable, obj_id);
    if (obj != NULL) {
    copy_cache_obj_to_request(req_local, obj);
    ARC_params->LRU2->remove(ARC_params->LRU2, obj_id);
    } else {
      ERROR("remove object %"PRIu64 "that is not cached\n", obj_id);
      return;
    }
  }

  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);
  cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;
}


#ifdef __cplusplus
}
#endif

//
//  Random.c
//  libCacheSim
//
//  Random eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../include/libCacheSim/evictionAlgo/Random.h"
#include "../dataStructure/hashtable/hashtable.h"


#ifdef __cplusplus
extern "C" {
#endif

cache_t *Random_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params) {
  if (ccache_params.hashpower == HASH_POWER_DEFAULT) {
    INFO("Please set hashpower parameter for Random to operate efficiently\n");
  }
  cache_t *cache = cache_struct_init("Random", ccache_params);
  cache->cache_init = Random_init;
  cache->cache_free = Random_free;
  cache->get = Random_get;
  cache->check = Random_check;
  cache->insert = Random_insert;
  cache->evict = Random_evict;
  cache->remove = Random_remove;

  srand((unsigned) time(NULL));
  set_rand_seed(time(NULL));
  return cache;
}

void Random_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e Random_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check(cache, req, update_cache, NULL);
}

cache_ck_res_e Random_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void Random_insert(cache_t *cache, request_t *req) {
  cache->occupied_size += req->obj_size + cache->per_obj_overhead;
  hashtable_insert(cache->hashtable, req);
}

void Random_evict(cache_t *cache, request_t *req, cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict = hashtable_rand_obj(cache->hashtable);
  DEBUG_ASSERT(obj_to_evict->obj_size != 0);
  if (cache_obj != NULL)
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);
  DEBUG_ASSERT(obj_to_evict->obj_size != 0);
  hashtable_delete(cache->hashtable, obj_to_evict);
}

void Random_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }
  cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  hashtable_delete(cache->hashtable, obj);
}

#ifdef __cplusplus
}
#endif

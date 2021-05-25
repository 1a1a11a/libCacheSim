//
//  Random.c
//  libCacheSim
//
//  Random evictionAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/evictionAlgo/Random.h"
#include "../dataStructure/hashtable/hashtable.h"
#include "../utils/include/mymath.h"

cache_t *Random_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params) {
  if (ccache_params.hashpower == HASH_POWER_DEFAULT) {
    INFO("Please set hashpower parameter for Random to operate efficiently\n");
  }
  cache_t *cache = cache_struct_init("Random", ccache_params);
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

#ifdef __cplusplus
}
#endif

//
//  Random.c
//  libMimircache
//
//  Random cacheAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include "Random.h"

#include "../utils/include/mathUtils.h"


cache_t *Random_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("Random", ccache_params);
  srand((unsigned) time(NULL));
  set_rand_seed(time(NULL));
  return cache;
}


void Random_free(cache_t *cache) {
//  g_free(cache->cache_params);
  cache_struct_free(cache);
}


cache_check_result_t Random_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check(cache, req, update_cache, NULL);
}


cache_check_result_t Random_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}


void _Random_insert(cache_t *cache, request_t *req) {
  cache->core.used_size += req->obj_size;
  hashtable_insert(cache->core.hashtable_new, req);
}


void _Random_evict(cache_t *cache, request_t *req, cache_obj_t* evicted_obj) {
  cache_obj_t *obj_to_evict = hashtable_rand_obj(cache->core.hashtable_new);
  if (evicted_obj != NULL)
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  hashtable_delete(cache->core.hashtable_new, obj_to_evict);
}


#ifdef __cplusplus
}
#endif

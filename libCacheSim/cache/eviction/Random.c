//
//  Random.c
//  libCacheSim
//
//  Random eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../../include/libCacheSim/evictionAlgo/Random.h"

#include "../../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *Random_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  if (ccache_params.hashpower == HASH_POWER_DEFAULT) {
    INFO("Please set hashpower parameter for Random to operate efficiently\n");
  }
  cache_t *cache = cache_struct_init("Random", ccache_params);
  cache->cache_init = Random_init;
  cache->cache_free = Random_free;
  cache->get = Random_get;
  cache->check = Random_check;
  cache->insert = Random_insert;
  cache->to_evict = Random_to_evict;
  cache->evict = Random_evict;
  cache->remove = Random_remove;
  cache->init_params = cache_specific_params;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  return cache;
}

void Random_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e Random_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
  return cache_check_base(cache, req, update_cache, NULL);
}

cache_ck_res_e Random_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *Random_insert(cache_t *cache, const request_t *req) {
  return cache_insert_base(cache, req);
}

cache_obj_t *Random_to_evict(cache_t *cache) {
  return hashtable_rand_obj(cache->hashtable);
}

void Random_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict = Random_to_evict(cache);
  DEBUG_ASSERT(obj_to_evict->obj_size != 0);
  if (cache_obj != NULL) memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  cache_remove_obj_base(cache, obj_to_evict);
}

void Random_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("obj to remove is not in the cache\n");
    return;
  }
  cache_remove_obj_base(cache, obj);
}

#ifdef __cplusplus
}
#endif

//
//  LRUv0.h
//  libCacheSim
//
//  Created by Juncheng on 10/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef LRUv0_H
#define LRUv0_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"

typedef struct LRUv0_params {
  GHashTable *hashtable;
  GQueue *list;
} LRUv0_params_t;

cache_t *LRUv0_init(common_cache_params_t ccache_params, void *cache_specific_params);

void LRUv0_free(cache_t *cache);

cache_ck_res_e LRUv0_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e LRUv0_get(cache_t *cache, request_t *req);

void LRUv0_insert(cache_t *LRUv0, request_t *req);

cache_obj_t *LRUv0_to_evict(cache_t *cache);

void LRUv0_evict(cache_t *LRUv0, request_t *req, cache_obj_t* cache_obj);

void LRUv0_remove(cache_t *cache, obj_id_t obj_id);


#ifdef __cplusplus
}
#endif


#endif  /* LRUv0_H */
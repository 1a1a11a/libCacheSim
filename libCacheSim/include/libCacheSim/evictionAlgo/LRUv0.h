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
extern "C" {
#endif

#include "../cache.h"

cache_t *LRUv0_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

void LRUv0_free(cache_t *cache);

cache_ck_res_e LRUv0_check(cache_t *cache, const request_t *req,
                           const bool update);

cache_ck_res_e LRUv0_get(cache_t *cache, const request_t *req);

cache_obj_t *LRUv0_insert(cache_t *LRUv0, const request_t *req);

cache_obj_t *LRUv0_to_evict(cache_t *cache);

void LRUv0_evict(cache_t *LRUv0, const request_t *req, cache_obj_t *cache_obj);

void LRUv0_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* LRUv0_H */
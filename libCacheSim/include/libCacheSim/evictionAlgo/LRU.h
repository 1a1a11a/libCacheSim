//
//  LRU.h
//  libCacheSim
//
//  Created by Juncheng on 10/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef LRU_H
#define LRU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"

cache_t *LRU_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void LRU_free(cache_t *cache);

cache_ck_res_e LRU_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e LRU_get(cache_t *cache, request_t *req);

void LRU_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

void LRU_insert(cache_t *LRU, request_t *req);

void LRU_evict(cache_t *LRU, request_t *req, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif  /* LRU_H */

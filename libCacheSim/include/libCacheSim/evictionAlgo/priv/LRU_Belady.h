//
//  LRU_Belady.h
//  libCacheSim
//
//  Created by Juncheng on 10/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef LRU_Belady_H
#define LRU_Belady_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../cache.h"

cache_t *LRU_Belady_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

void *LRU_Belady_params_parser(char *params);

void LRU_Belady_free(cache_t *cache);

bool LRU_Belady_check(cache_t *cache, const request_t *req, const bool update);

bool LRU_Belady_get(cache_t *cache, const request_t *req);

cache_obj_t *LRU_Belady_insert(cache_t *cache, const request_t *req);

cache_obj_t *LRU_Belady_to_evict(cache_t *cache);

void LRU_Belady_evict(cache_t *cache, const request_t *req,
                      cache_obj_t *evicted_obj);

bool LRU_Belady_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* LRU_Belady_H */

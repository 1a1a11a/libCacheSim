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
extern "C" {
#endif

#include "../cache.h"

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} LRU_params_t;

cache_t *LRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

void *LRU_params_parser(char *params);

void LRU_free(cache_t *cache);

bool LRU_check(cache_t *cache, const request_t *req, const bool update);

bool LRU_get(cache_t *cache, const request_t *req);

cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);

cache_obj_t *LRU_to_evict(cache_t *cache);

void LRU_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

bool LRU_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* LRU_H */

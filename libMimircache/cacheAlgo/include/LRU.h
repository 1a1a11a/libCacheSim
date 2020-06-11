//
//  LRU.h
//  libMimircache
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

#include "../../include/mimircache/cache.h"


//typedef struct LRU_params {
//  cache_obj_t* lru_head;
//  cache_obj_t* lru_tail;
//} LRU_params_t;

cache_t *LRU_init(common_cache_params_t ccache_params, void *cache_specific_params);

void LRU_free(cache_t *cache);

cache_check_result_t LRU_check(cache_t *cache, request_t *req, bool update);

cache_check_result_t LRU_get(cache_t *cache, request_t *req);

void LRU_remove_obj(cache_t *cache, request_t *req);

void _LRU_insert(cache_t *LRU, request_t *req);

void _LRU_evict(cache_t *LRU, request_t *req, cache_obj_t* cache_obj);


//cache_check_result_t LRU_get_with_ttl(cache_t* cache, request_t *req);
//
//cache_check_result_t LRU_check_with_ttl(cache_t* cache, request_t *req, bool update);
//



#ifdef __cplusplus
}
#endif


#endif  /* LRU_H */

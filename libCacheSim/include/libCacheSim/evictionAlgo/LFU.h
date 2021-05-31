//
//  LFU.h
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFU_H
#define LFU_H


#include "../cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


cache_t *LFU_init(common_cache_params_t ccache_params,
                      void *cache_specific_params);

void LFU_free(cache_t *cache);

cache_ck_res_e LFU_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e LFU_get(cache_t *cache, request_t *req);

void LFU_remove(cache_t *cache, obj_id_t obj_id);

void LFU_insert(cache_t *LFU, request_t *req);

void LFU_evict(cache_t *LFU, request_t *req, cache_obj_t *cache_obj);


#ifdef __cplusplus
}
#endif


#endif	/* LFU_H */

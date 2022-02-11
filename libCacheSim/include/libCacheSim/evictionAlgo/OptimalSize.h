#pragma once
//
//  OptimalSize.h
//  libCacheSim
//

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"

cache_t *OptimalSize_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params);

void OptimalSize_free(cache_t *cache);

void OptimalSize_insert(cache_t *OptimalSize, request_t *req);

cache_ck_res_e OptimalSize_check(cache_t *cache, request_t *req, bool update_cache);

cache_obj_t *OptimalSize_to_evict(cache_t *cache);

void OptimalSize_evict(cache_t *OptimalSize, request_t *req, cache_obj_t *cache_obj);

cache_ck_res_e OptimalSize_get(cache_t *cache, request_t *req);

void OptimalSize_remove(cache_t *cache, obj_id_t obj_id);


#ifdef __cplusplus
}

#endif


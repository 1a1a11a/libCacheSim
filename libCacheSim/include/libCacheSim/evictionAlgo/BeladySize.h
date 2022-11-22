#pragma once
//
//  BeladySize.h
//  libCacheSim
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"

cache_t *BeladySize_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_init_params);

void BeladySize_free(cache_t *cache);

cache_obj_t *BeladySize_insert(cache_t *BeladySize, const request_t *req);

cache_ck_res_e BeladySize_check(cache_t *cache, const request_t *req,
                                const bool update_cache);

cache_obj_t *BeladySize_to_evict(cache_t *cache);

void BeladySize_evict(cache_t *BeladySize, const request_t *req,
                      cache_obj_t *cache_obj);

cache_ck_res_e BeladySize_get(cache_t *cache, const request_t *req);

void BeladySize_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}

#endif

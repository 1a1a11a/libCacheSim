#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

cache_t *LeCaR_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void LeCaR_free(cache_t *cache);

cache_ck_res_e LeCaR_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e LeCaR_get(cache_t *cache, request_t *req);

void LeCaR_insert(cache_t *LeCaR, request_t *req);

void LeCaR_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void LeCaR_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif



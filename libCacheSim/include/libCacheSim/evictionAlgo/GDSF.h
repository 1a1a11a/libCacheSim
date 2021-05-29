#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

cache_t *GDSF_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void GDSF_free(cache_t *cache);

cache_ck_res_e GDSF_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e GDSF_get(cache_t *cache, request_t *req);

void GDSF_insert(cache_t *GDSF, request_t *req);

void GDSF_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void GDSF_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif



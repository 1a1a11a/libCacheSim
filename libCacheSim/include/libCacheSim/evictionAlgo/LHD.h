#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LHD_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

void LHD_free(cache_t *cache);

cache_ck_res_e LHD_check(cache_t *cache, const request_t *req,
                         const bool update_cache);

cache_ck_res_e LHD_get(cache_t *cache, const request_t *req);

cache_obj_t *LHD_insert(cache_t *LHD, const request_t *req);

void LHD_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

void LHD_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

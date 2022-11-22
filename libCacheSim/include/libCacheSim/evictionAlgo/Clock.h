#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *Clock_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

void Clock_free(cache_t *cache);

cache_ck_res_e Clock_check(cache_t *cache, const request_t *req,
                           const bool update_cache);

cache_ck_res_e Clock_get(cache_t *cache, const request_t *req);

cache_obj_t *Clock_insert(cache_t *Clock, const request_t *req);

cache_obj_t *Clock_to_evict(cache_t *cache);

void Clock_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj);

void Clock_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

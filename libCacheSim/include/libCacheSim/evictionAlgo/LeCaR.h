#pragma once

#include "../cache.h"
#include "LFU.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LeCaR_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

void LeCaR_free(cache_t *cache);

bool LeCaR_check(cache_t *cache, const request_t *req, const bool update_cache);

bool LeCaR_get(cache_t *cache, const request_t *req);

cache_obj_t *LeCaR_insert(cache_t *LeCaR, const request_t *req);

cache_obj_t *LeCaR_to_evict(cache_t *cache);

void LeCaR_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj);

bool LeCaR_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

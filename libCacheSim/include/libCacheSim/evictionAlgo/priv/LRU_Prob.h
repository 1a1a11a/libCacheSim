#pragma once

#include "../../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LRU_Prob_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

void LRU_Prob_free(cache_t *cache);

bool LRU_Prob_check(cache_t *cache, const request_t *req,
                    const bool update_cache);

bool LRU_Prob_get(cache_t *cache, const request_t *req);

cache_obj_t *LRU_Prob_insert(cache_t *cache, const request_t *req);

cache_obj_t *LRU_Prob_to_evict(cache_t *cache);

void LRU_Prob_evict(cache_t *cache, const request_t *req,
                    cache_obj_t *evicted_obj);

bool LRU_Prob_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

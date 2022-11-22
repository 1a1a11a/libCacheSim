#pragma once
/**
 * @file LeCaRv0.h
 * @author Juncheng Yang
 * @brief a LeCaR implementation using four small caches, this is slower than
 * LeCaR
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LeCaRv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

void LeCaRv0_free(cache_t *cache);

cache_ck_res_e LeCaRv0_check(cache_t *cache, const request_t *req,
                             const bool update_cache);

cache_ck_res_e LeCaRv0_get(cache_t *cache, const request_t *req);

cache_obj_t *LeCaRv0_insert(cache_t *LeCaRv0, const request_t *req);

cache_obj_t *LeCaRv0_to_evict(cache_t *cache);

void LeCaRv0_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj);

void LeCaRv0_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

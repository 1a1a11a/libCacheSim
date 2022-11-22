#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *Hyperbolic_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

void Hyperbolic_free(cache_t *cache);

cache_ck_res_e Hyperbolic_check(cache_t *cache, const request_t *req,
                                const bool update_cache);

cache_ck_res_e Hyperbolic_get(cache_t *cache, const request_t *req);

cache_obj_t *Hyperbolic_insert(cache_t *Hyperbolic, const request_t *req);

cache_obj_t *Hyperbolic_to_evict(cache_t *cache);

void Hyperbolic_evict(cache_t *cache, const request_t *req,
                      cache_obj_t *evicted_obj);

void Hyperbolic_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *lazyFIFO_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

void lazyFIFO_free(cache_t *cache);

cache_ck_res_e lazyFIFO_check(cache_t *cache, const request_t *req,
                              const bool update_cache);

cache_ck_res_e lazyFIFO_get(cache_t *cache, const request_t *req);

cache_obj_t *lazyFIFO_insert(cache_t *lazyFIFO, const request_t *req);

cache_obj_t *lazyFIFO_to_evict(cache_t *cache);

void lazyFIFO_evict(cache_t *cache, const request_t *req,
                    cache_obj_t *evicted_obj);

void lazyFIFO_remove(cache_t *cache, const obj_id_t obj_id);

cache_t *lazyFIFOv2_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

void lazyFIFOv2_free(cache_t *cache);

cache_ck_res_e lazyFIFOv2_check(cache_t *cache, const request_t *req,
                              const bool update_cache);

cache_ck_res_e lazyFIFOv2_get(cache_t *cache, const request_t *req);

cache_obj_t *lazyFIFOv2_insert(cache_t *lazyFIFOv2, const request_t *req);

cache_obj_t *lazyFIFOv2_to_evict(cache_t *cache);

void lazyFIFOv2_evict(cache_t *cache, const request_t *req,
                    cache_obj_t *evicted_obj);

void lazyFIFOv2_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif


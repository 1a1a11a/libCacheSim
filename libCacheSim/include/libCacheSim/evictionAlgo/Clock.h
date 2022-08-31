#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *pointer;
} Clock_params_t;

cache_t *Clock_init(common_cache_params_t ccache_params,
                    void *cache_specific_params);

void Clock_free(cache_t *cache);

cache_ck_res_e Clock_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e Clock_get(cache_t *cache, request_t *req);

void Clock_insert(cache_t *Clock, request_t *req);

cache_obj_t *Clock_to_evict(cache_t *cache);

void Clock_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void Clock_remove(cache_t *cache, obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

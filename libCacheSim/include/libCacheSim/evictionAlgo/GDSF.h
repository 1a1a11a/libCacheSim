#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *GDSF_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void GDSF_free(cache_t *cache);

cache_ck_res_e GDSF_check(cache_t *cache, const request_t *req,
                          const bool update_cache);

cache_ck_res_e GDSF_get(cache_t *cache, const request_t *req);

cache_obj_t *GDSF_insert(cache_t *GDSF, const request_t *req);

void GDSF_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

void GDSF_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

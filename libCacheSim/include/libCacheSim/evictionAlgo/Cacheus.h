#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif


cache_t *Cacheus_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

void Cacheus_free(cache_t *cache);

cache_ck_res_e Cacheus_check(cache_t *cache, const request_t *req,
                             const bool update_cache);

cache_ck_res_e Cacheus_get(cache_t *cache, const request_t *req);

cache_obj_t *Cacheus_insert(cache_t *Cacheus, const request_t *req);

cache_obj_t *Cacheus_to_evict(cache_t *cache);

void Cacheus_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj);

void Cacheus_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

void Cacheus_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

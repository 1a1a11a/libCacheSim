#pragma once

#include "../../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *MyClock_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

void MyClock_free(cache_t *cache);

bool MyClock_check(cache_t *cache, const request_t *req, const bool update_cache);

bool MyClock_get(cache_t *cache, const request_t *req);

cache_obj_t *MyClock_insert(cache_t *Clock, const request_t *req);

cache_obj_t *MyClock_to_evict(cache_t *cache);

void MyClock_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj);

bool MyClock_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

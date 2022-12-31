//
//  LPQD.h
//
//  
//

#ifndef LPQD_H
#define LPQD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../cache.h"

cache_t *LPQD_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void LPQD_free(cache_t *cache);

bool LPQD_check(cache_t *cache, const request_t *req, const bool update);

bool LPQD_get(cache_t *cache, const request_t *req);

cache_obj_t *LPQD_insert(cache_t *cache, const request_t *req);

cache_obj_t *LPQD_to_evict(cache_t *cache);

void LPQD_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

bool LPQD_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* LPQD_H */

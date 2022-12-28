//
//  SLRUv0.h
//  libCacheSim
//

#ifndef SLRUv0_H
#define SLRUv0_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"
#include "LRU.h"

cache_t *SLRUv0_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

void SLRUv0_free(cache_t *cache);

bool SLRUv0_check(cache_t *cache, const request_t *req, const bool update);

bool SLRUv0_get(cache_t *cache, const request_t *req);

cache_obj_t *SLRUv0_insert(cache_t *cache, const request_t *req);

cache_obj_t *SLRUv0_to_evict(cache_t *cache);

void SLRUv0_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj);

bool SLRUv0_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* SLRUv0_H */

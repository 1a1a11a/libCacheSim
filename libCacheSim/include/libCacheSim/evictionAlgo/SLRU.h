//
//  SLRU.h
//  libCacheSim
//

#ifndef SLRU_H
#define SLRU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"
#include "LRU.h"

cache_t *SLRU_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void SLRU_free(cache_t *cache);

cache_ck_res_e SLRU_check(cache_t *cache, const request_t *req,
                          const bool update);

cache_ck_res_e SLRU_get(cache_t *cache, const request_t *req);

void SLRU_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *SLRU_insert(cache_t *cache, const request_t *req);

cache_obj_t *SLRU_to_evict(cache_t *cache);

void SLRU_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

#ifdef __cplusplus
}
#endif

#endif /* SLRU_H */

//
//  SLRU.h
//  libCacheSim
//

#ifndef SLRU_H
#define SLRU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"
#include "LRU.h"

typedef struct SLRU_params {
  cache_t **LRUs;
  int n_seg;
} SLRU_params_t;


typedef struct SLRU_init_params {
  int n_seg;
} SLRU_init_params_t;


cache_t *SLRU_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void SLRU_free(cache_t *cache);

cache_ck_res_e SLRU_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e SLRU_get(cache_t *cache, request_t *req);

void SLRU_remove(cache_t *cache, obj_id_t obj_id);

void SLRU_insert(cache_t *cache, request_t *req);

void SLRU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

#ifdef __cplusplus
}
#endif

#endif  /* SLRU_H */

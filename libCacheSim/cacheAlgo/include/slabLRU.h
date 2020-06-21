//
// evict the least recent accessed slab 
// 
// Created by Juncheng Yang on 04/22/20.
//

#ifndef libCacheSim_SLABLRU_H
#define libCacheSim_SLABLRU_H

#ifdef __cplusplus
"C"
{
#endif

#include "slabCommon.h"
#include "../../include/libCacheSim/cache.h"

typedef struct slabLRU_params {
  GHashTable *hashtable;
  slab_params_t slab_params;
} slabLRU_params_t;

cache_t *slabLRU_init(common_cache_params_t ccache_params,
                      void *cache_specific_init_params);

void slabLRU_free(cache_t *cache);

cache_ck_res_e slabLRU_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e slabLRU_get(cache_t *cache, request_t *req);

void slabLRU_insert(cache_t *cache, request_t *req);

void slabLRU_update(cache_t *cache, request_t *req);

void slabLRU_evict(cache_t *cache, request_t *req, cache_obj_t *cache_obj);

void slabLRU_remove_obj(cache_t *cache, void *req);

#ifdef __cplusplus
}
#endif

#endif //libCacheSim_SLABLRU_H

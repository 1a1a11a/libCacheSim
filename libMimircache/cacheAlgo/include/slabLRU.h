//
// evict the least recent accessed slab 
// 
// Created by Juncheng Yang on 04/22/20.
//

#ifndef MIMIRCACHE_SLABLRU_H
#define MIMIRCACHE_SLABLRU_H


#ifdef __cplusplus
"C"
{
#endif

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"
#include "slabCommon.h"


typedef struct slabLRU_params {
  GHashTable *hashtable;
  slab_params_t slab_params;
} slabLRU_params_t;


cache_t *slabLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params);

void slabLRU_free(cache_t *cache);

gboolean slabLRU_check(cache_t *cache, request_t *req);

gboolean slabLRU_get(cache_t *cache, request_t *req);

void _slabLRU_insert(cache_t *cache, request_t *req);

void _slabLRU_update(cache_t *cache, request_t *req);

void _slabLRU_evict(cache_t *cache, request_t *req);

gpointer slabLRU_evict_with_return(cache_t *cache, request_t *req);


void slabLRU_remove_obj(cache_t *cache, void *data_to_remove);

GHashTable *slabLRU_get_objmap(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif //MIMIRCACHE_SLABLRU_H

//
// evict the least recent created slab
//
// Created by Juncheng Yang on 04/22/20.
//

#ifndef libCacheSim_SLABLRC_H
#define libCacheSim_SLABLRC_H


#ifdef __cplusplus
"C"
{
#endif

#include "../../include/libCacheSim/cache.h"
#include "utilsInternal.h"
#include "slabCommon.h"


typedef struct slabLRC_params {
//  uint64_t ts;
  GHashTable *hashtable;
  slab_params_t slab_params;
} slabLRC_params_t;


cache_t *slabLRC_init(common_cache_params_t ccache_params, void *cache_specific_init_params);

void slabLRC_free(cache_t *cache);

gboolean slabLRC_check(cache_t *cache, request_t *req);

gboolean slabLRC_get(cache_t *cache, request_t *req);

void _slabLRC_insert(cache_t *cache, request_t *req);

void _slabLRC_update(cache_t *cache, request_t *req);

void _slabLRC_evict(cache_t *cache, request_t *req);

gpointer slabLRC_evict_with_return(cache_t *cache, request_t *req);


void slabLRC_remove_obj(cache_t *cache, void *data_to_remove);

GHashTable *slabLRC_get_objmap(cache_t *cache);


#ifdef __cplusplus
}
#endif

#endif //libCacheSim_SLABLRC_H

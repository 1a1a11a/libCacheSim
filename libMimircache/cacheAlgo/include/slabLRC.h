//
// evict the least recent created slab
//
// Created by Juncheng Yang on 04/22/20.
//

#ifndef MIMIRCACHE_SLABLRC_H
#define MIMIRCACHE_SLABLRC_H


#ifdef __cplusplus
"C"
{
#endif

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"
#include "slabCommon.h"


typedef struct slabLRC_params {
  GHashTable *hashtable;
  slab_params_t slab_params;
} slabLRC_params_t;


cache_t *slabLRC_init(common_cache_params_t ccache_params, void *cache_specific_init_params);

void slabLRC_free(cache_t *cache);

cache_check_result_t slabLRC_check(cache_t *cache, request_t *req, bool update_cache);

cache_check_result_t slabLRC_get(cache_t *cache, request_t *req);

void _slabLRC_insert(cache_t *cache, request_t *req);

void _slabLRC_update(cache_t *cache, request_t *req);

void _slabLRC_evict(cache_t *cache, request_t *req, cache_obj_t* evicted_obj);


void slabLRC_remove_obj(cache_t *cache, request_t *req);


#ifdef __cplusplus
}
#endif

#endif //MIMIRCACHE_SLABLRC_H

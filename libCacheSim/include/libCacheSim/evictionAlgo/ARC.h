//
//  ARC cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//


#ifndef ARC_h
#define ARC_h

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"


typedef struct ARC_params {
  cache_t *LRU1;      // LRU
  cache_t *LRU1g;     // ghost LRU
  cache_t *LRU2;      // LRU for items accessed more than once
  cache_t *LRU2g;     // ghost LRU for items accessed more than once3
  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int evict_lru;              // which LRU list the eviction should come from
} ARC_params_t;


typedef struct ARC_init_params {
  double ghost_list_factor;
} ARC_init_params_t;

/* initialize ARC, the default ghost list size is the same as the cache size */
cache_t *ARC_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void ARC_free(cache_t *cache);

cache_ck_res_e ARC_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e ARC_get(cache_t *cache, request_t *req);

void ARC_insert(cache_t *ARC, request_t *req);

cache_obj_t *ARC_to_evict(cache_t *cache);

void ARC_evict(cache_t *ARC, request_t *req, cache_obj_t *cache_obj);

void ARC_remove(cache_t *cache, obj_id_t obj_id);


#ifdef __cplusplus
}
#endif


#endif  /* ARC_H */

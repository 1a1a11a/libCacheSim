//
//  SLRU.h
//  libCacheSim
//
//  Created by Juncheng on 2/12/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#ifndef SLRU_h
#define SLRU_h


#include "../../include/libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo/LRU.h"
#include "utilsInternal.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct SLRU_params {
  cache_t **LRUs;
  int n_seg;
  uint64_t *current_sizes;
} SLRU_params_t;


typedef struct SLRU_init_params {
  int n_seg;
} SLRU_init_params_t;


extern gboolean SLRU_check(cache_t *cache, request_t *cp);

extern gboolean SLRU_add(cache_t *cache, request_t *cp);


extern void _SLRU_insert(cache_t *SLRU, request_t *cp);

extern void _SLRU_update(cache_t *SLRU, request_t *cp);

extern void _SLRU_evict(cache_t *SLRU, request_t *cp);

extern void *_SLRU_evict_with_return(cache_t *SLRU, request_t *cp);


extern void SLRU_destroy(cache_t *cache);

extern void SLRU_destroy_unique(cache_t *cache);


cache_t *SLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params);


extern void SLRU_remove_obj(cache_t *cache, void *data_to_remove);

extern guint64 SLRU_get_size(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif  /* SLRU_H */

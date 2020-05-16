//
//  LRU.h
//  libMimircache
//
//  Created by Juncheng on 10/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef LRU_H
#define LRU_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/mimircache/cache.h"


typedef struct LRU_params {
  GHashTable *hashtable;
  GQueue *list;
} LRU_params_t;

cache_t *LRU_init(common_cache_params_t ccache_params, void *cache_specific_params);

extern void LRU_free(cache_t *cache);

extern gboolean LRU_check(cache_t *cache, request_t *req);

extern gboolean LRU_get(cache_t *cache, request_t *req);

extern cache_obj_t *LRU_get_cached_obj(cache_t *cache, request_t *req);

extern void LRU_remove_obj(cache_t *cache, void *data_to_remove);


extern void _LRU_insert(cache_t *LRU, request_t *req);

extern void _LRU_update(cache_t *LRU, request_t *req);

extern void _LRU_evict(cache_t *LRU, request_t *req);

extern void *_LRU_evict_with_return(cache_t *LRU, request_t *req);


gboolean LRU_get_with_ttl(cache_t* cache, request_t *req);


#ifdef __cplusplus
}
#endif


#endif  /* LRU_H */

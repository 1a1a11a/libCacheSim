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
//    gint64 logical_ts;              // this only works when add is called
} LRU_params_t;

cache_t *LRU_init(guint64 size, obj_id_t obj_id_type, void *params);

extern void LRU_free(cache_t *cache);

extern gboolean LRU_check(cache_t *cache, request_t *req);

extern gboolean LRU_add(cache_t *cache, request_t *req);

extern cache_obj_t *LRU_get_cached_obj(cache_t *cache, request_t *req);

extern void LRU_remove_obj(cache_t *cache, void *data_to_remove);



extern void _LRU_insert(cache_t *LRU, request_t *req);

extern void _LRU_update(cache_t *LRU, request_t *req);

extern void _LRU_evict(cache_t *LRU, request_t *req);

extern void *_LRU_evict_with_return(cache_t *LRU, request_t *req);


#ifdef __cplusplus
}
#endif


#endif  /* LRU_H */

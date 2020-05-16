//
//  FIFO.h
//  libMimircache
//
//  Created by Juncheng on 5/08/19.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFO_SIZE_H
#define FIFO_SIZE_H


#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
"C"
{
#endif


typedef struct FIFO_params {
  GHashTable *hashtable;
  GQueue *list;
} FIFO_params_t;


cache_t *FIFO_init(common_cache_params_t ccache_params, void *cache_specific_params);

void FIFO_free(cache_t *cache);

gboolean FIFO_check(cache_t *cache, request_t *req);

gboolean FIFO_get(cache_t *cache, request_t *req);

void _FIFO_insert(cache_t *FIFO, request_t *req);

void _FIFO_update(cache_t *FIFO, request_t *req);

void _FIFO_evict(cache_t *FIFO, request_t *req);

void *_FIFO_evict_with_return(cache_t *FIFO, request_t *req);

void FIFO_remove_obj(cache_t *cacheAlg, void *obj_id_ptr);


gboolean FIFO_get_with_ttl(cache_t *cache, request_t *req);


#ifdef __cplusplus
}
#endif


#endif  /* FIFO_SIZE_H */

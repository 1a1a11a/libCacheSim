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
extern "C"
{
#endif


typedef struct FIFO_params {
  GHashTable *hashtable;
  GQueue *list;
} FIFO_params_t;


extern cache_t *FIFO_init(guint64 size, obj_id_type_t obj_id_type, void *params);

extern void FIFO_free(cache_t *cache);

extern gboolean FIFO_check(cache_t *cache, request_t *req);
gboolean FIFO_check_with_ttl(cache_t *cache, request_t* req);


extern gboolean FIFO_get(cache_t *cache, request_t *req);

extern void _FIFO_insert(cache_t *FIFO, request_t *req);

extern void _FIFO_update(cache_t *FIFO, request_t *req);

extern void _FIFO_evict(cache_t *FIFO, request_t *req);

extern void *_FIFO_evict_with_return(cache_t *FIFO, request_t *req);

extern void FIFO_remove_obj(cache_t *cacheAlg, void *obj_id_ptr);


#ifdef __cplusplus
}
#endif


#endif  /* FIFO_SIZE_H */

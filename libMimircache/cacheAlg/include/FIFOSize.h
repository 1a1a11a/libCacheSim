//
//  FIFOSize.h
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


struct FIFOSize_params {
  GHashTable *hashtable;
  GQueue *list;
  gint64 ts;              // this only works when add is called
};


typedef struct FIFOSize_params FIFOSize_params_t;


extern gboolean FIFOSize_check(cache_t *cache, request_t *req);
extern gboolean FIFOSize_add(cache_t *cache, request_t *req);
extern void _FIFOSize_insert(cache_t *FIFOSize, request_t *req);
extern void _FIFOSize_update(cache_t *FIFOSize, request_t *req);
extern void _FIFOSize_evict(cache_t *FIFOSize, request_t *req);
extern void *_FIFOSize_evict_with_return(cache_t *FIFOSize, request_t *req);
extern void FIFOSize_destroy(cache_t *cache);
extern void FIFOSize_destroy_unique(cache_t *cache);
cache_t *FIFOSize_init(guint64 size, obj_id_t obj_id_type, void *params);
cache_t *FIFO_init(guint64 size, obj_id_t obj_id_type, void *params);

//extern void FIFOSize_remove_obj(cache_t *cacheAlg, void *data_to_remove);

extern guint64 FIFOSize_get_used_size(cache_t *cache);
extern GHashTable *FIFOSize_get_objmap(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif  /* FIFO_SIZE_H */

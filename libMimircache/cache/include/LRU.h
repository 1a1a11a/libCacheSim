//
//  LRU.h
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LRU_h
#define LRU_h


#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct LRU_params{
    GHashTable *hashtable;
    GQueue *list;
    gint64 ts;              // this only works when add is called
};

typedef struct LRU_params LRU_params_t; 




extern gboolean LRU_check(cache_t* cache, request_t* req);
extern gboolean LRU_add(cache_t* cache, request_t* req);


extern void     _LRU_insert(cache_t* LRU, request_t* req);
extern void     _LRU_update(cache_t* LRU, request_t* req);
extern void     _LRU_evict(cache_t* LRU, request_t* req);
extern void*    _LRU_evict_with_return(cache_t* LRU, request_t* req);


extern void     LRU_destroy(cache_t* cache);
extern void     LRU_destroy_unique(cache_t* cache);


cache_t*   LRU_init(guint64 size, obj_id_t obj_id_type, void* params);


extern void     LRU_remove_obj(cache_t* cache, void* data_to_remove);
extern guint64 LRU_get_size(cache_t* cache);


#ifdef __cplusplus
}
#endif


#endif	/* LRU_H */

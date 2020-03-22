//
//  LFU.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFU_h
#define LFU_h



#include "../../include/mimircache/cache.h"
#include "pqueue.h"
#include "utilsInternal.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct LFU_params{
    GHashTable *hashtable;
    pqueue_t *pq;
//    gint64 req_cnt;
};
typedef struct LFU_params LFU_params_t;





extern gboolean LFU_check(cache_t* cache, request_t* req);
extern gboolean LFU_add(cache_t* cache, request_t* req);


extern void     _LFU_insert(cache_t* LFU, request_t* req);
extern void     _LFU_update(cache_t* LFU, request_t* req);
extern void     _LFU_evict(cache_t* LFU, request_t* req);
extern void*    _LFU_evict_with_return(cache_t* cache, request_t* req);


extern void     LFU_destroy(cache_t* cache);
extern void     LFU_destroy_unique(cache_t* cache);

cache_t*   LFU_init(guint64 size, obj_id_t obj_id_type, void* params);


extern void     LFU_remove_obj(cache_t* cache, void* data_to_remove);
extern guint64 LFU_get_size(cache_t* cache);


#ifdef __cplusplus
}
#endif


#endif	/* LFU_H */ 

//
//  myLRU.h
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef myLRU_h
#define myLRU_h


#include "mimircache.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct myLRU_params{
    GHashTable *hashtable;
    GQueue *list;
    gint64 ts;              // this only works when add_element is called 
};

typedef struct myLRU_params myLRU_params_t; 




extern gboolean myLRU_check_element(cache_t* cache, request_t* req);
extern gboolean myLRU_add_element(cache_t* cache, request_t* req);


extern void     __myLRU_insert_element(cache_t* myLRU, request_t* req);
extern void     __myLRU_update_element(cache_t* myLRU, request_t* req);
extern void     __myLRU_evict_element(cache_t* myLRU, request_t* req);
extern void*    __myLRU__evict_with_return(cache_t* myLRU, request_t* req);


extern void     myLRU_destroy(cache_t* cache);
extern void     myLRU_destroy_unique(cache_t* cache);


cache_t*   myLRU_init(guint64 size, obj_id_t obj_id_type, guint64 block_size, void* params);


extern void     myLRU_remove_element(cache_t* cache, void* data_to_remove);
extern guint64 myLRU_get_size(cache_t* cache);


#ifdef __cplusplus
}
#endif


#endif	/* myLRU_H */

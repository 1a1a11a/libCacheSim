//
//  LFUFast.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFUFast_H
#define LFUFast_H


#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct LFUFast_params{
    GHashTable *hashtable;      // key -> glist 
    GQueue *main_list;
    gint min_freq; 
}LFUFast_params_t;


// main list is a doubly linkedlist sort by frequency
typedef struct main_list_node_data{
    gint freq;
    GQueue *queue;   // linked to branch list
}main_list_node_data_t;


// branch list is the list of items with same freq, sorted in LRU
typedef struct branch_list_node_data{
    gpointer key;
    GList *main_list_node;
    
}branch_list_node_data_t;



extern gboolean LFUFast_check(cache_t* cache, request_t* cp);
extern gboolean LFUFast_add(cache_t* cache, request_t* cp);


extern void     _LFUFast_insert(cache_t* LFUFast, request_t* cp);
extern void     _LFUFast_update(cache_t* LFUFast, request_t* cp);
extern void     _LFUFast_evict(cache_t* LFUFast, request_t* cp);
extern void*    _LFUFast_evict_with_return(cache_t* cache, request_t* cp);


extern void     LFUFast_destroy(cache_t* cache);
extern void     LFUFast_destroy_unique(cache_t* cache);

cache_t*   LFUFast_init(guint64 size, obj_id_t obj_id_type, void* params);


extern void     LFUFast_remove_obj(cache_t* cache, void* data_to_remove);
extern guint64 LFUFast_get_size(cache_t* cache);


#ifdef __cplusplus
}
#endif


#endif	/* LFUFast_H */

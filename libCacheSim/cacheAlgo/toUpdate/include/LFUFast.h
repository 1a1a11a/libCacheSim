//
//  LFUFast.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFUFast_H
#define LFUFast_H


#include "../../include/libCacheSim/cache.h"
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


// branch list is the list of items with same freq, sorted in LFUFast
typedef struct branch_list_node_data{
    gpointer key;
    GList *main_list_node;
    
}branch_list_node_data_t;


cache_t *LFUFast_init(common_cache_params_t ccache_params, void *cache_specific_params);

extern void LFUFast_free(cache_t *cache);

extern gboolean LFUFast_check(cache_t *cache, request_t *req);

extern gboolean LFUFast_get(cache_t *cache, request_t *req);

extern cache_obj_t *LFUFast_get_cached_obj(cache_t *cache, request_t *req);

extern void LFUFast_remove_obj(cache_t *cache, void *data_to_remove);


extern void _LFUFast_insert(cache_t *LFUFast, request_t *req);

extern void _LFUFast_update(cache_t *LFUFast, request_t *req);

extern void _LFUFast_evict(cache_t *LFUFast, request_t *req);

extern void *_LFUFast_evict_with_return(cache_t *LFUFast, request_t *req);


gboolean LFUFast_get_with_ttl(cache_t* cache, request_t *req);


#ifdef __cplusplus
}
#endif


#endif	/* LFUFast_H */

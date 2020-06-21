

//
//  LRU_K.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LRU_K_H
#define LRU_K_H


#ifdef __cplusplus
extern "C"
{
#endif


#define INITIAL_TS 0 


#include "../../include/libCacheSim/cache.h"
#include "pqueue.h" 
#include "utilsInternal.h"



typedef struct LRU_K_params{
    GHashTable *cache_hashtable;        // obj_id -> pq_node_pointer
    GHashTable *ghost_hashtable;        // obj_id -> gqueue of size K
    pqueue_t *pq;                       
    guint K;
    guint maxK;
    guint64 ts;
}LRU_K_params_t;

typedef struct LRU_K_init_params{
    int K;
    int maxK;
}LRU_K_init_params_t;



extern  void _LRU_K_insert(cache_t* LRU_K, request_t* req);

extern  gboolean LRU_K_check(cache_t* cache, request_t* req);

extern  void _LRU_K_update(cache_t* LRU_K, request_t* req);

extern  void _LRU_K_evict(cache_t* LRU_K);

extern  gboolean LRU_K_add(cache_t* cache, request_t* req);

extern  void LRU_K_destroy(cache_t* cache);
extern  void LRU_K_destroy_unique(cache_t* cache);


cache_t* LRU_K_init(common_cache_params_t ccache_params, void *cache_specific_init_params);


#ifdef __cplusplus
}
#endif


#endif	/* LRU_K_H */

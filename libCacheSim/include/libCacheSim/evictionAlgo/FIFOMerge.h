//
//  FIFOMergeMerge.h
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFOMerge_SIZE_H
#define FIFOMerge_SIZE_H

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct fifo_merge_sort_list_node {
    double metric; 
    cache_obj_t *cache_obj; 
}; 

typedef struct FIFOMerge_params {
    cache_obj_t *next_to_merge; 
    int n_merge_obj; 
    int n_keep_obj; 
    struct fifo_merge_sort_list_node *metric_list; 
    bool use_oracle; 
} FIFOMerge_params_t;

typedef struct FIFOMerge_init_params {
    int n_keep_obj; 
    int n_merge_obj; 
    bool use_oracle; 
} FIFOMerge_init_params_t;

cache_t *FIFOMerge_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void FIFOMerge_free(cache_t *cache);

cache_ck_res_e FIFOMerge_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e FIFOMerge_get(cache_t *cache, request_t *req);

void FIFOMerge_insert(cache_t *FIFOMerge, request_t *req);

void FIFOMerge_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void FIFOMerge_remove(cache_t *cache, obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif  /* FIFOMerge_SIZE_H */

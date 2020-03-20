//
//  LRUSize.h
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LRU_SIZE_H
#define LRU_SIZE_H


#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct LRUSize_params{
    GHashTable *hashtable;
    GQueue *list;
    gint64 logical_ts;              // this only works when add is called

#ifdef TRACK_EVICTION_AGE
  GHashTable *last_access_rtime_map;
  GHashTable *last_access_vtime_map;
  FILE* eviction_age_ofile;

  int eviction_cnt;
  long long logical_eviction_age_sum;
  long long real_eviction_age_sum;
  long long last_output_ts;
#endif

}LRUSize_params_t;


extern gboolean LRUSize_check(cache_t* cache, request_t* req);
extern gboolean LRUSize_add(cache_t* cache, request_t* req);
extern void* LRUSize_get_cached_data(cache_t *cache, request_t *req);
extern void LRUSize_update_cached_data(cache_t* cache, request_t* req, void* extra_data);

extern void     _LRUSize_insert(cache_t* LRUSize, request_t* req);
extern void     _LRUSize_update(cache_t* LRUSize, request_t* req);
extern void     _LRUSize_evict(cache_t* LRUSize, request_t* req);
extern void*    _LRUSize_evict_with_return(cache_t* LRUSize, request_t* req);


extern void     LRUSize_destroy(cache_t* cache);
extern void     LRUSize_destroy_unique(cache_t* cache);


cache_t*   LRUSize_init(guint64 size, obj_id_t obj_id_type, void* params);


extern void     LRUSize_remove_obj(cache_t* cache, void* data_to_remove);
extern guint64   LRUSize_get_size(cache_t* cache);
extern GHashTable* LRUSize_get_objmap();


#ifdef __cplusplus
}
#endif


#endif	/* LRU_SIZE_H */ 

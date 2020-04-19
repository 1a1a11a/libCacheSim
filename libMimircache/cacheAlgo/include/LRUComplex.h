//
// Created by Juncheng Yang on 3/20/20.
//

#ifndef LIBMIMIRCACHE_LRUCOMPLEX_H
#define LIBMIMIRCACHE_LRUCOMPLEX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"


typedef struct LRUComplex_params{
  GHashTable *hashtable;
  GQueue *list;
//    gint64 logical_ts;              // this only works when add is called

#ifdef TRACK_EVICTION_AGE
  GHashTable *last_access_rtime_map;
  GHashTable *last_access_vtime_map;
  FILE* eviction_age_ofile;

  int eviction_cnt;
  long long logical_eviction_age_sum;
  long long real_eviction_age_sum;
  long long last_output_ts;
#endif

}LRUComplex_params_t;


extern gboolean LRUComplex_check(cache_t* cache, request_t* req);
extern gboolean LRUComplex_add(cache_t* cache, request_t* req);
extern void* LRUComplex_get_cached_data(cache_t *cache, request_t *req);
extern void LRUComplex_update_cached_data(cache_t* cache, request_t* req, void* extra_data);

extern void     _LRUComplex_insert(cache_t* LRUComplex, request_t* req);
extern void     _LRUComplex_update(cache_t* LRUComplex, request_t* req);
extern void     _LRUComplex_evict(cache_t* LRUComplex, request_t* req);
extern void*    _LRUComplex_evict_with_return(cache_t* LRUComplex, request_t* req);


extern void     LRUComplex_destroy(cache_t* cache);
extern void     LRUComplex_destroy_unique(cache_t* cache);


cache_t*   LRUComplex_init(guint64 size, obj_id_type_t obj_id_type, void* params);


extern void     LRUComplex_remove_obj(cache_t* cache, void* data_to_remove);
extern guint64   LRUComplex_get_used_size(cache_t* cache);
extern GHashTable* LRUComplex_get_objmap();


#ifdef __cplusplus
}
#endif



#endif //LIBMIMIRCACHE_LRUCOMPLEX_H

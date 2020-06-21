//
//  myLRU.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef myLRU_h
#define myLRU_h


#include "libCacheSim.h"
// #include "../../libCacheSim/include/libCacheSim.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct myLRU_params {
  GHashTable *hashtable;
  GQueue *list;
  gint64 ts;              // this only works when add_element is called
};

typedef struct myLRU_params myLRU_params_t;


cache_t *myLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params);

extern void myLRU_free(cache_t *cache);

extern gboolean myLRU_check(cache_t *cache, request_t *req);

extern gboolean myLRU_add(cache_t *cache, request_t *req);

extern cache_obj_t *myLRU_get_cached_obj(cache_t *cache, request_t *req);

extern void myLRU_remove_obj(cache_t *cache, void *data_to_remove);


extern void _myLRU_insert(cache_t *cache, request_t *req);

extern void _myLRU_update(cache_t *cache, request_t *req);

extern void _myLRU_evict(cache_t *cache, request_t *req);

extern void *_myLRU_evict_with_return(cache_t *cache, request_t *req);


#ifdef __cplusplus
}
#endif


#endif  /* myLRU_H */

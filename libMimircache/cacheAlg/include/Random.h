//
//  Random.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef Random_h
#define Random_h

#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct Random_params {
  GHashTable *hashtable;
  GArray *array;
};


extern void _Random_insert(cache_t *Random, request_t *req);

extern gboolean Random_check(cache_t *cache, request_t *req);

extern void _Random_update(cache_t *Random, request_t *req);

extern void _Random_evict(cache_t *Random, request_t *req);

extern gboolean Random_add(cache_t *cache, request_t *req);

extern void Random_destroy(cache_t *cache);

extern void Random_destroy_unique(cache_t *cache);

cache_t *Random_init(guint64 size, obj_id_t obj_id_type, void *params);


#ifdef __cplusplus
}

#endif


#endif /* Random_h */

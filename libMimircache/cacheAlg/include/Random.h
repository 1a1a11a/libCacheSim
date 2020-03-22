//
//  Random.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef Random_h
#define Random_h

#ifdef __cplusplus
extern "C"
{
#endif


#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"


typedef struct Random_params {
  GHashTable *hashtable;
  GPtrArray *array;
}Random_params_t;


cache_t *Random_init(guint64 size, obj_id_t obj_id_type, void *params);

extern void Random_free(cache_t *cache);

extern void _Random_insert(cache_t *Random, request_t *req);

extern gboolean Random_check(cache_t *cache, request_t *req);

extern void _Random_update(cache_t *Random, request_t *req);

extern void _Random_evict(cache_t *Random, request_t *req);

extern gboolean Random_get(cache_t *cache, request_t *req);


#ifdef __cplusplus
}

#endif


#endif /* Random_h */

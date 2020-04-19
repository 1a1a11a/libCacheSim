//
//  MRU.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef MRU_h
#define MRU_h

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct MRU_params {
  GHashTable *hashtable;
};


extern void _MRU_insert(cache_t *MRU, request_t *req);

extern gboolean MRU_check(cache_t *cache, request_t *req);

extern void _MRU_update(cache_t *MRU, request_t *req);

extern void _MRU_evict(cache_t *MRU, request_t *req);

extern gboolean MRU_add(cache_t *cache, request_t *req);

extern void MRU_destroy(cache_t *cache);

extern void MRU_destroy_unique(cache_t *cache);

cache_t *MRU_init(guint64 size, obj_id_type_t obj_id_type, void *params);


#ifdef __cplusplus
}
#endif


#endif /* MRU_h */

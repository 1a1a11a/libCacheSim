//
//  ARC.h
//  libCacheSim
//
//  Created by Juncheng on 2/12/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#ifndef ARC_h
#define ARC_h


#include "../../include/libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo/LRU.h"
//#include "LFU.h"
#include "../../utils/include/utilsInternal.h"



#ifdef __cplusplus
extern "C"
{
#endif


// by default, the ghost list size is 10 times the orginal evictionAlgo size

typedef struct ARC_params {
  cache_t *LRU1;         // normal LRU segment
  cache_t *LRU1g;        // ghost list for normal LRU segment
  cache_t *LRU2;         // normal LRU segement for items accessed more than once
  cache_t *LRU2g;        // ghost list for normal LFU segment
  gint32 ghost_list_factor;  // size(ghost_list)/size(evictionAlgo),
  // by default, the ghost list size is
  // 10 times the orginal evictionAlgo size
  gint64 size1;             // size for segment 1
  gint64 size2;             // size for segment 2
} ARC_params_t;


typedef struct ARC_init_params {
  gint32 ghost_list_factor;
} ARC_init_params_t;


extern gboolean ARC_check(cache_t *cache, request_t *req);

extern gboolean ARC_add_element(cache_t *cache, request_t *req);


extern void _ARC_insert_element(cache_t *ARC, request_t *req);

extern void _ARC_update_element(cache_t *ARC, request_t *req);

extern void _ARC_evict_element(cache_t *ARC, request_t *req);

extern void* _ARC_evict_with_return(cache_t *ARC, request_t *req);


extern void ARC_destroy(cache_t *cache);

extern void ARC_destroy_unique(cache_t *cache);


cache_t *ARC_init(common_cache_params_t ccache_params, void *cache_specific_params);


extern void ARC_remove_obj(cache_t *cache, void *data_to_remove);

extern guint64 ARC_get_size(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif  /* ARC_H */ 

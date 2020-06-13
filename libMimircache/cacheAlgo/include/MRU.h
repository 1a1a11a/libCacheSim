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


#ifdef __cplusplus
extern "C"
{
#endif



void _MRU_insert(cache_t *cache, request_t *req);

cache_check_result_t MRU_check(cache_t *cache, request_t *req, bool update_cache);

void _MRU_evict(cache_t *cache, request_t *req, cache_obj_t* cache_obj);

cache_check_result_t MRU_get(cache_t *cache, request_t *req);

void MRU_destroy(cache_t *cache);

cache_t *MRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params);


#ifdef __cplusplus
}
#endif


#endif /* MRU_h */

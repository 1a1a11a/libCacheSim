//
//  Random.h
//  libCacheSim
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

#include "../../include/libCacheSim/cache.h"
  
cache_t *Random_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params);

void Random_free(cache_t *cache);

void Random_insert(cache_t *Random, request_t *req);

cache_ck_res_e Random_check(cache_t *cache, request_t *req, bool update_cache);

void Random_evict(cache_t *Random, request_t *req, cache_obj_t *cache_obj);

cache_ck_res_e Random_get(cache_t *cache, request_t *req);

#ifdef __cplusplus
}

#endif

#endif /* Random_h */

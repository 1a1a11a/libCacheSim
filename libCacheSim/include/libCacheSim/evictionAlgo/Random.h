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
extern "C" {
#endif

#include "../cache.h"

cache_t *Random_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_init_params);

void Random_free(cache_t *cache);

cache_obj_t *Random_insert(cache_t *Random, const request_t *req);

cache_ck_res_e Random_check(cache_t *cache, const request_t *req,
                            const bool update_cache);

cache_obj_t *Random_to_evict(cache_t *cache);

void Random_evict(cache_t *Random, const request_t *req,
                  cache_obj_t *cache_obj);

cache_ck_res_e Random_get(cache_t *cache, const request_t *req);

void Random_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}

#endif

#endif /* Random_h */

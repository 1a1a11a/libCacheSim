//
//  LFUCpp.h
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFUCpp_H
#define LFUCpp_H

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LFUCpp_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

void LFUCpp_free(cache_t *cache);

cache_ck_res_e LFUCpp_check(cache_t *cache, const request_t *req,
                         const bool update);

cache_ck_res_e LFUCpp_get(cache_t *cache, const request_t *req);

void LFUCpp_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *LFUCpp_insert(cache_t *LFUCpp, const request_t *req);

cache_obj_t *LFUCpp_to_evict(cache_t *cache);

void LFUCpp_evict(cache_t *LFUCpp, const request_t *req, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif /* LFUCpp_H */

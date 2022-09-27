//
//  FIFOMergeMerge.h
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFOMerge_SIZE_H
#define FIFOMerge_SIZE_H

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *FIFOMerge_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);

void FIFOMerge_free(cache_t *cache);

cache_ck_res_e FIFOMerge_check(cache_t *cache, const request_t *req,
                               const bool update_cache);

cache_ck_res_e FIFOMerge_get(cache_t *cache, const request_t *req);

void FIFOMerge_insert(cache_t *FIFOMerge, const request_t *req);

cache_obj_t *FIFOMerge_to_evict(cache_t *cache);

void FIFOMerge_evict(cache_t *cache, const request_t *req,
                     cache_obj_t *evicted_obj);

void FIFOMerge_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

void FIFOMerge_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* FIFOMerge_SIZE_H */

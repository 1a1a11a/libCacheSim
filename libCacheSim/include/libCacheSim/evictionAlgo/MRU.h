//
//  MRU.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef MRU_h
#define MRU_h

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_obj_t *MRU_insert(cache_t *cache, const request_t *req);

cache_ck_res_e MRU_check(cache_t *cache, const request_t *req,
                         const bool update_cache);

cache_obj_t *MRU_to_evict(cache_t *cache);

void MRU_evict(cache_t *cache, const request_t *req, cache_obj_t *cache_obj);

cache_ck_res_e MRU_get(cache_t *cache, const request_t *req);

void MRU_free(cache_t *cache);

cache_t *MRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_init_params);

void MRU_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* MRU_h */

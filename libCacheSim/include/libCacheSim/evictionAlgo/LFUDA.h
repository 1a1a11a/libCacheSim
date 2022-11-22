//
//  LFUDA.h
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFUDA_H
#define LFUDA_H

#include "../cache.h"
#include "LFU.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LFUDA_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

void LFUDA_free(cache_t *cache);

cache_ck_res_e LFUDA_check(cache_t *cache, const request_t *req,
                           const bool update);

cache_ck_res_e LFUDA_get(cache_t *cache, const request_t *req);

void LFUDA_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *LFUDA_insert(cache_t *LFUDA, const request_t *req);

cache_obj_t *LFUDA_to_evict(cache_t *cache);

void LFUDA_evict(cache_t *LFUDA, const request_t *req, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif /* LFUDA_H */

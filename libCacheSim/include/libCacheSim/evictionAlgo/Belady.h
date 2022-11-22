//
//  Belady.h
//  libCacheSim
//
//  Created by Juncheng on 3/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *Belady_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_init_params);

void Belady_free(cache_t *cache);

cache_obj_t *Belady_insert(cache_t *Belady, const request_t *req);

cache_ck_res_e Belady_check(cache_t *cache, const request_t *req,
                            const bool update_cache);

cache_obj_t *Belady_to_evict(cache_t *cache);

void Belady_evict(cache_t *Belady, __attribute__((unused)) const request_t *req,
                  __attribute__((unused)) cache_obj_t *cache_obj);

cache_ck_res_e Belady_get(cache_t *cache, const request_t *req);

void Belady_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}

#endif

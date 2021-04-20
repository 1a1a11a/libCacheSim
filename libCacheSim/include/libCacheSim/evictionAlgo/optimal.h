//
//  optimal.h
//  libCacheSim
//
//  Created by Juncheng on 3/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"
#include "../../../dataStructure/include/pqueue.h"

typedef struct optimal_params {
  pqueue_t *pq;
} optimal_params_t;


cache_t *optimal_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params);

void optimal_free(cache_t *cache);

void optimal_insert(cache_t *optimal, request_t *req);

cache_ck_res_e optimal_check(cache_t *cache, request_t *req, bool update_cache);

void optimal_evict(cache_t *optimal, request_t *req, cache_obj_t *cache_obj);

cache_ck_res_e optimal_get(cache_t *cache, request_t *req);

void optimal_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

#ifdef __cplusplus
}

#endif


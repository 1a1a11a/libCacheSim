//
//  Optimal.h
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
#include "../../../dataStructure/pqueue.h"

typedef struct Optimal_params {
  pqueue_t *pq;
} Optimal_params_t;

cache_t *Optimal_init(common_cache_params_t ccache_params,
                      __attribute__((unused)) void *cache_specific_init_params);

void Optimal_free(cache_t *cache);

void Optimal_insert(cache_t *Optimal, request_t *req);

cache_ck_res_e Optimal_check(cache_t *cache, request_t *req, bool update_cache);

void Optimal_evict(cache_t *Optimal,
                   __attribute__((unused)) request_t *req,
                   __attribute__((unused)) cache_obj_t *cache_obj);

cache_ck_res_e Optimal_get(cache_t *cache, request_t *req);

void Optimal_remove(cache_t *cache, obj_id_t obj_id);

#ifdef __cplusplus
}

#endif


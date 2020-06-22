//
//  FIFO.h
//  libCacheSim
//
//  Created by Juncheng on 5/08/19.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFO_SIZE_H
#define FIFO_SIZE_H

#include "../cache.h"

#ifdef __cplusplus
"C"
{
#endif

cache_t *FIFO_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void FIFO_free(cache_t *cache);

cache_ck_res_e FIFO_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e FIFO_get(cache_t *cache, request_t *req);

void FIFO_insert(cache_t *FIFO, request_t *req);

void FIFO_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

/* TODO (jason) update interface */
void FIFO_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif  /* FIFO_SIZE_H */

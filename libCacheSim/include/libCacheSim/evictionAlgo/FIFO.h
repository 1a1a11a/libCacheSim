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
extern "C" {
#endif

cache_t *FIFO_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void FIFO_free(cache_t *cache);

cache_ck_res_e FIFO_check(cache_t *cache, const request_t *req,
                          const bool update_cache);

cache_ck_res_e FIFO_get(cache_t *cache, const request_t *req);

cache_obj_t *FIFO_insert(cache_t *FIFO, const request_t *req);

cache_obj_t *FIFO_to_evict(cache_t *cache);

void FIFO_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

void FIFO_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_SIZE_H */

//
//  FIFO.h
//  libMimircache
//
//  Created by Juncheng on 5/08/19.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFO_SIZE_H
#define FIFO_SIZE_H


#include "../../include/mimircache/cache.h"


#ifdef __cplusplus
"C"
{
#endif


//typedef struct FIFO_params {
//  cache_obj_t* fifo_head;
//  cache_obj_t* fifo_tail;
//} FIFO_params_t;


cache_t *FIFO_init(common_cache_params_t ccache_params, void *cache_specific_params);

void FIFO_free(cache_t *cache);

cache_check_result_t FIFO_check(cache_t *cache, request_t *req, bool update_cache);

cache_check_result_t FIFO_get(cache_t *cache, request_t *req);

void _FIFO_insert(cache_t *FIFO, request_t *req);

void _FIFO_evict(cache_t *cache, request_t *req, cache_obj_t* evicted_obj);

void FIFO_remove_obj(cache_t *cache, request_t* req);


cache_check_result_t FIFO_get_with_ttl(cache_t *cache, request_t *req);

cache_check_result_t FIFO_check_with_ttl(cache_t *cache, request_t *req, bool update_cache);


#ifdef __cplusplus
}
#endif


#endif  /* FIFO_SIZE_H */

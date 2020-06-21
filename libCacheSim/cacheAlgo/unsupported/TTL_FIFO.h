//
//  a TTL_FIFO module that eviction objects according to TTL, if same TTL, evict using FIFO
//
//  TTL_FIFO.h
//  libCacheSim
//
//  Created by Juncheng on 5/08/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#ifndef TTL_FIFO_SIZE_H
#define TTL_FIFO_SIZE_H


#include "../../include/libCacheSim/cache.h"


#ifdef __cplusplus
"C"
{
#endif


#define MAX_TTL 5184000   // 60 day

typedef struct TTL_FIFO_params {
  gint32 start_ts;
  GHashTable *hashtable;
  GQueue *exp_time_array[MAX_TTL];     // array of GQueue ptr, each points to obj expires at time T
  gint32 cur_ttl_evict_pos;
} TTL_FIFO_params_t;


cache_t *TTL_FIFO_init(common_cache_params_t ccache_params, void *cache_specific_params);

void TTL_FIFO_free(cache_t *cache);

gboolean TTL_FIFO_check(cache_t *cache, request_t *req);

gboolean TTL_FIFO_get(cache_t *cache, request_t *req);

void _TTL_FIFO_insert(cache_t *TTL_FIFO, request_t *req);

void _TTL_FIFO_update(cache_t *TTL_FIFO, request_t *req);

void _TTL_FIFO_evict(cache_t *TTL_FIFO, request_t *req);

void *_TTL_FIFO_evict_with_return(cache_t *TTL_FIFO, request_t *req);

void TTL_FIFO_remove_obj(cache_t *cacheAlg, void *obj_id_ptr);


gboolean TTL_FIFO_get_with_ttl(cache_t *cache, request_t *req);


#ifdef __cplusplus
}
#endif


#endif  /* TTL_FIFO_SIZE_H */

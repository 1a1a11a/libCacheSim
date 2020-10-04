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
extern "C"
{
#endif


typedef struct LFUDA_params {
//  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;
//  uint64_t age;
} LFUDA_params_t;


cache_t *LFUDA_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void LFUDA_free(cache_t *cache);

cache_ck_res_e LFUDA_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e LFUDA_get(cache_t *cache, request_t *req);

void LFUDA_remove(cache_t *cache, obj_id_t obj_id);

void LFUDA_insert(cache_t *LFUDA, request_t *req);

void LFUDA_evict(cache_t *LFUDA, request_t *req, cache_obj_t *cache_obj);


#ifdef __cplusplus
}
#endif


#endif	/* LFUDA_H */

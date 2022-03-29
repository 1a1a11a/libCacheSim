//
//  LFUFast.h
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFUFast_H
#define LFUFast_H


#include "../cache.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct freq_node {
  uint32_t freq;
  uint32_t n_obj;
  cache_obj_t *first_obj;
  cache_obj_t *last_obj;
} freq_node_t;

typedef struct LFUFast_params {
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;
} LFUFast_params_t;


cache_t *LFUFast_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void LFUFast_free(cache_t *cache);

cache_ck_res_e LFUFast_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e LFUFast_get(cache_t *cache, request_t *req);

void LFUFast_remove(cache_t *cache, obj_id_t obj_id);

void LFUFast_insert(cache_t *LFUFast, request_t *req);

cache_obj_t *LFUFast_to_evict(cache_t *cache);

void LFUFast_evict(cache_t *LFUFast, request_t *req, cache_obj_t *cache_obj);


#ifdef __cplusplus
}
#endif


#endif	/* LFUFast_H */

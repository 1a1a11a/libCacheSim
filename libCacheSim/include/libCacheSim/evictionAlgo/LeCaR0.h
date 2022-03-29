#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct LeCaR0_params {
  cache_t *LRU;      // LRU
  cache_t *LRU_g;     // eviction history of LRU
  cache_t *LFU;      // LFU
  cache_t *LFU_g;     // eviction history of LFU
  double w_lru;
  double w_lfu;
  double lr;        // learning rate
  double dr;        // discount rate
  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int64_t n_hit_lru_history;
  int64_t n_hit_lfu_history;
} LeCaR0_params_t;

cache_t *LeCaR0_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void LeCaR0_free(cache_t *cache);

cache_ck_res_e LeCaR0_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e LeCaR0_get(cache_t *cache, request_t *req);

void LeCaR0_insert(cache_t *LeCaR0, request_t *req);

cache_obj_t *LeCaR0_to_evict(cache_t *cache);

void LeCaR0_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void LeCaR0_remove(cache_t *cache, obj_id_t obj_id);


#ifdef __cplusplus
}
#endif



#pragma once

#include "../cache.h"
#include "LFUFast.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LeCaR_params {

  // LRU chain pointers are part of cache_t struct 
  
  // used for LFU 
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;

  // eviction history 
  cache_obj_t *ghost_lru_head;
  cache_obj_t *ghost_lru_tail;
  int64_t ghost_entry_used_size;

  // LeCaR 
  double w_lru;
  double w_lfu;
  double lr;        // learning rate
  double dr;        // discount rate
  // double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int64_t n_hit_lru_history;
  int64_t n_hit_lfu_history;
} LeCaR_params_t;

cache_t *LeCaR_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void LeCaR_free(cache_t *cache);

cache_ck_res_e LeCaR_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e LeCaR_get(cache_t *cache, request_t *req);

void LeCaR_insert(cache_t *LeCaR, request_t *req);

cache_obj_t *LeCaR_to_evict(cache_t *cache);

void LeCaR_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void LeCaR_remove(cache_t *cache, obj_id_t obj_id);


#ifdef __cplusplus
}
#endif



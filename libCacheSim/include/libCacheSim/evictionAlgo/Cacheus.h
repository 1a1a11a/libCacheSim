#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Cacheus_params {
  cache_t *LRU;      // LRU
  cache_t *LRU_g;     // eviction history of LRU
  cache_t *LFU;      // LFU
  cache_t *LFU_g;     // eviction history of LFU
  double w_lru;     // Weight for LRU
  double w_lfu;     // Weight for LFU
  double lr;        // learning rate
  double lr_previous;  // previous learning rate

  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int64_t unlearn_count;

  int64_t num_hit;
  double hit_rate_prev; 

  uint64_t update_interval;
} Cacheus_params_t;

cache_t *Cacheus_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void Cacheus_free(cache_t *cache);

cache_ck_res_e Cacheus_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e Cacheus_get(cache_t *cache, request_t *req);

void Cacheus_insert(cache_t *Cacheus, request_t *req);

cache_obj_t *Cacheus_to_evict(cache_t *cache);

void Cacheus_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void Cacheus_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

void Cacheus_remove(cache_t *cache, obj_id_t obj_id);

#ifdef __cplusplus
}
#endif



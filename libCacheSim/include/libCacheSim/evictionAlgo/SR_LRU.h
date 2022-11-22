#ifndef SR_LRU_H
#define SR_LRU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"

typedef struct SR_LRU_params {
  cache_t *SR_list;    // Scan Resistant list
  cache_t *R_list;     // Churn Resistant List
  cache_t *H_list;     // History
  uint64_t C_demoted;  // count of demoted object in cache
  uint64_t C_new;      // count of new item in history
  cache_t *other_cache;
} SR_LRU_params_t;

cache_t *SR_LRU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

void SR_LRU_free(cache_t *cache);

cache_ck_res_e SR_LRU_check(cache_t *cache, const request_t *req,
                            const bool update);

cache_ck_res_e SR_LRU_get(cache_t *cache, const request_t *req);

void SR_LRU_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *SR_LRU_insert(cache_t *cache, const request_t *req);

cache_obj_t *SR_LRU_to_evict(cache_t *cache);

void SR_LRU_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj);

#ifdef __cplusplus
}
#endif

#endif /* LRU_H */

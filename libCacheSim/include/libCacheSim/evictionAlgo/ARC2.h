//
//  ARC2 cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#ifndef ARC2_h
#define ARC2_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"

typedef struct ARC2_params {
  cache_obj_t *lru1_head;
  cache_obj_t *lru1_tail;
  cache_obj_t *lru2_head;
  cache_obj_t *lru2_tail;

  cache_obj_t *lru1_ghost_tail;
  cache_obj_t *lru2_ghost_tail;

  uint64_t ghost_list_size;
  int evict_source;  // which LRU list the eviction should come from
  uint64_t ghost_list1_occupied_size;
  uint64_t ghost_list2_occupied_size;

  uint64_t lru1_n_obj;
  uint64_t lru2_n_obj;
  uint64_t ghost_list1_n_obj;
  uint64_t ghost_list2_n_obj;
} ARC2_params_t;

typedef struct ARC2_init_params {
  double ghost_list_size_ratio;
} ARC2_init_params_t;

/* initialize ARC2, the default ghost list size is the same as the cache size */
cache_t *ARC2_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void ARC2_free(cache_t *cache);

cache_ck_res_e ARC2_check(cache_t *cache, const request_t *req,
                          const bool update);

cache_ck_res_e ARC2_get(cache_t *cache, const request_t *req);

void ARC2_insert(cache_t *ARC2, const request_t *req);

cache_obj_t *ARC2_to_evict(cache_t *cache);

void ARC2_evict(cache_t *ARC2, const request_t *req, cache_obj_t *cache_obj);

void ARC2_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* ARC2_H */

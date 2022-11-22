//
//  LFU.h
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef LFU_H
#define LFU_H

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct freq_node {
  uint32_t freq;
  uint32_t n_obj;
  cache_obj_t *first_obj;
  cache_obj_t *last_obj;
} freq_node_t;

cache_t *LFU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

void LFU_free(cache_t *cache);

cache_ck_res_e LFU_check(cache_t *cache, const request_t *req,
                         const bool update);

cache_ck_res_e LFU_get(cache_t *cache, const request_t *req);

void LFU_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *LFU_insert(cache_t *LFU, const request_t *req);

cache_obj_t *LFU_to_evict(cache_t *cache);

void LFU_evict(cache_t *LFU, const request_t *req, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif /* LFU_H */

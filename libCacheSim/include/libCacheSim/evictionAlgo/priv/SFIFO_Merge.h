//
//  SFIFO_MergeMerge.h
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef SFIFO_Merge_SIZE_H
#define SFIFO_Merge_SIZE_H

#include "../../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *SFIFO_Merge_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params);

void SFIFO_Merge_free(cache_t *cache);

bool SFIFO_Merge_check(cache_t *cache, const request_t *req,
                       const bool update_cache);

bool SFIFO_Merge_get(cache_t *cache, const request_t *req);

cache_obj_t *SFIFO_Merge_insert(cache_t *SFIFO_Merge, const request_t *req);

cache_obj_t *SFIFO_Merge_to_evict(cache_t *cache);

void SFIFO_Merge_evict(cache_t *cache, const request_t *req,
                       cache_obj_t *evicted_obj);

void SFIFO_Merge_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

bool SFIFO_Merge_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* SFIFO_Merge_SIZE_H */

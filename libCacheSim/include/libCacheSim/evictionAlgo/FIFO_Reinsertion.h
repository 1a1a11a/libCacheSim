//
//  FIFO-readmission.h
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef FIFO_Reinsertion_SIZE_H
#define FIFO_Reinsertion_SIZE_H

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *FIFO_Reinsertion_init(const common_cache_params_t ccache_params,
                               const char *cache_specific_params);

void FIFO_Reinsertion_free(cache_t *cache);

cache_ck_res_e FIFO_Reinsertion_check(cache_t *cache, const request_t *req,
                                      const bool update_cache);

cache_ck_res_e FIFO_Reinsertion_get(cache_t *cache, const request_t *req);

cache_obj_t *FIFO_Reinsertion_insert(cache_t *FIFO_Reinsertion,
                                     const request_t *req);

cache_obj_t *FIFO_Reinsertion_to_evict(cache_t *cache);

void FIFO_Reinsertion_evict(cache_t *cache, const request_t *req,
                            cache_obj_t *evicted_obj);

void FIFO_Reinsertion_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

bool FIFO_Reinsertion_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_Reinsertion_SIZE_H */

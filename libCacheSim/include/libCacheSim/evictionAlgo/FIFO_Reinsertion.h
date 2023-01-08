#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *FIFO_Reinsertion_init(const common_cache_params_t ccache_params,
                               const char *cache_specific_params);

void FIFO_Reinsertion_free(cache_t *cache);

bool FIFO_Reinsertion_check(cache_t *cache, const request_t *req,
                            const bool update_cache);

bool FIFO_Reinsertion_get(cache_t *cache, const request_t *req);

cache_obj_t *FIFO_Reinsertion_insert(cache_t *cache, const request_t *req);

cache_obj_t *FIFO_Reinsertion_to_evict(cache_t *cache);

void FIFO_Reinsertion_evict(cache_t *cache, const request_t *req,
                            cache_obj_t *evicted_obj);

bool FIFO_Reinsertion_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif
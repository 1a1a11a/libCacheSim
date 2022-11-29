//
//  SFIFO_Belady.h
//
//  a segment FIFO eviction algorithm which reduce per request operation (lazy
//  promotion), and supports quick demotion libCacheSim
//

#ifndef SFIFO_Belady_H
#define SFIFO_Belady_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"
#include "FIFO.h"

cache_t *SFIFO_Belady_init(const common_cache_params_t ccache_params,
                           const char *cache_specific_params);

void SFIFO_Belady_free(cache_t *cache);

cache_ck_res_e SFIFO_Belady_check(cache_t *cache, const request_t *req,
                                  const bool update);

cache_ck_res_e SFIFO_Belady_get(cache_t *cache, const request_t *req);

void SFIFO_Belady_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *SFIFO_Belady_insert(cache_t *cache, const request_t *req);

cache_obj_t *SFIFO_Belady_to_evict(cache_t *cache);

void SFIFO_Belady_evict(cache_t *cache, const request_t *req,
                        cache_obj_t *evicted_obj);

#ifdef __cplusplus
}
#endif

#endif /* SFIFO_Belady_H */

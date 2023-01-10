//
//  QDLP.h
//
//

#ifndef QDLP_H
#define QDLP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../cache.h"

cache_t *QDLP_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

void QDLP_free(cache_t *cache);

bool QDLP_check(cache_t *cache, const request_t *req, const bool update);

bool QDLP_get(cache_t *cache, const request_t *req);

cache_obj_t *QDLP_insert(cache_t *cache, const request_t *req);

cache_obj_t *QDLP_to_evict(cache_t *cache);

void QDLP_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj);

bool QDLP_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

#endif /* QDLP_H */

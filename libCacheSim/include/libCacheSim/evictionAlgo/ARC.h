//
//  ARC cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#ifndef ARC_h
#define ARC_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../cache.h"

/* initialize ARC, the default ghost list size is the same as the cache size */
cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

void ARC_free(cache_t *cache);

cache_ck_res_e ARC_check(cache_t *cache, const request_t *req,
                         const bool update);

cache_ck_res_e ARC_get(cache_t *cache, const request_t *req);

cache_obj_t *ARC_insert(cache_t *ARC, const request_t *req);

cache_obj_t *ARC_to_evict(cache_t *cache);

void ARC_evict(cache_t *ARC, const request_t *req, cache_obj_t *cache_obj);

void ARC_remove(cache_t *cache, obj_id_t const obj_id);

#ifdef __cplusplus
}
#endif

#endif /* ARC_H */

#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
  int associativity;
  int admission;
} LHD_init_params_t;

typedef struct {

  void *LHD_cache;

  int associativity;
  int admission;

} LHD_params_t;

cache_t *LHD_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void LHD_free(cache_t *cache);

cache_ck_res_e LHD_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e LHD_get(cache_t *cache, request_t *req);

void LHD_insert(cache_t *LHD, request_t *req);

void LHD_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

/* TODO (jason) update interface */
void LHD_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif


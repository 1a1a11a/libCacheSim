#pragma once

#include "../../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LPv1_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void LPv1_free(cache_t *cache);

bool LPv1_check(cache_t *cache, const request_t *req, const bool update_cache);

bool LPv1_get(cache_t *cache, const request_t *req);

cache_obj_t *LPv1_insert(cache_t *cache, const request_t *req);

cache_obj_t *LPv1_to_evict(cache_t *cache);

void LPv1_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

bool LPv1_remove(cache_t *cache, const obj_id_t obj_id);

cache_t *LPv2_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void LPv2_free(cache_t *cache);

bool LPv2_check(cache_t *cache, const request_t *req, const bool update_cache);

bool LPv2_get(cache_t *cache, const request_t *req);

cache_obj_t *LPv2_insert(cache_t *cache, const request_t *req);

cache_obj_t *LPv2_to_evict(cache_t *cache);

void LPv2_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj);

bool LPv2_remove(cache_t *cache, const obj_id_t obj_id);

cache_t *LPv2_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

void QDLPv1_free(cache_t *cache);

bool QDLPv1_check(cache_t *cache, const request_t *req,
                  const bool update_cache);

bool QDLPv1_get(cache_t *cache, const request_t *req);

cache_obj_t *QDLPv1_insert(cache_t *cache, const request_t *req);

cache_obj_t *QDLPv1_to_evict(cache_t *cache);

void QDLPv1_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj);

bool QDLPv1_remove(cache_t *cache, const obj_id_t obj_id);

cache_t *QDLPv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

void QDLPv2_free(cache_t *cache);

bool QDLPv2_check(cache_t *cache, const request_t *req,
                  const bool update_cache);

bool QDLPv2_get(cache_t *cache, const request_t *req);

cache_obj_t *QDLPv2_insert(cache_t *cache, const request_t *req);

cache_obj_t *QDLPv2_to_evict(cache_t *cache);

void QDLPv2_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj);

bool QDLPv2_remove(cache_t *cache, const obj_id_t obj_id);

cache_t *QDLPv2_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

#ifdef __cplusplus
}
#endif

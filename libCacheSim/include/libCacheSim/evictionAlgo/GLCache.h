//
//  GLCache.h
//  libCacheSim
//

#pragma once

#include "../cache.h"
#include <xgboost/c_api.h>
typedef float feature_t;
typedef float pred_t;
typedef float train_y_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SEGCACHE = 0,
  LOGCACHE_BOTH_ORACLE = 1,
  LOGCACHE_LOG_ORACLE = 2,
  LOGCACHE_ITEM_ORACLE = 3,  // FIFO for seg selection
  LOGCACHE_LEARNED = 4,
} GLCache_type_e;

typedef enum obj_score_type {
  OBJ_SCORE_FREQ = 0,
  OBJ_SCORE_FREQ_BYTE = 1,
  OBJ_SCORE_FREQ_AGE = 2,
  OBJ_SCORE_FREQ_AGE_BYTE = 3,

  OBJ_SCORE_HIT_DENSITY = 4,

  OBJ_SCORE_ORACLE = 5,
  OBJ_SCORE_SIZE_AGE = 6,
} obj_score_type_e;

typedef enum bucket_type {
  NO_BUCKET = 0,

  SIZE_BUCKET = 1,
  TTL_BUCKET = 2,
  CUSTOMER_BUCKET = 3,
  BUCKET_ID_BUCKET = 4,
  CONTENT_TYPE_BUCKET = 5,
} bucket_type_e;

typedef enum training_source {
  TRAIN_Y_FROM_ONLINE,
  TRAIN_Y_FROM_ORACLE,
} train_source_e;

cache_t *GLCache_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

void GLCache_free(cache_t *cache);

cache_ck_res_e GLCache_check(cache_t *cache, const request_t *req,
                             const bool update_cache);

cache_ck_res_e GLCache_get(cache_t *cache, const request_t *req);

void GLCache_insert(cache_t *GLCache, const request_t *req);

void GLCache_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj);

void GLCache_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

void GLCache_remove(cache_t *cache, const obj_id_t obj_id);

#ifdef __cplusplus
}
#endif

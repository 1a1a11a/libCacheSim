#pragma once

#include "../../../include/libCacheSim/cacheObj.h"
#include "GLCacheInternal.h"

static inline void obj_init(cache_t *cache, const request_t *req,
                            cache_obj_t *cache_obj, segment_t *seg) {
  GLCache_params_t *params = cache->eviction_params;
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->GLCache.freq = 0;
  cache_obj->GLCache.last_access_rtime = req->clock_time;
  cache_obj->GLCache.last_access_vtime = params->curr_vtime;
  cache_obj->GLCache.in_cache = 1;
  cache_obj->GLCache.seen_after_snapshot = 0;
  cache_obj->misc.next_access_vtime = req->next_access_vtime;

  cache_obj->GLCache.segment = seg;
  cache_obj->GLCache.idx_in_segment = seg->n_obj;
}

static inline int64_t obj_age(GLCache_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->GLCache.last_access_rtime;
}

/* some internal state update when an object is requested */
static inline void obj_hit_update(GLCache_params_t *params, cache_obj_t *obj,
                                  const request_t *req) {
  obj->misc.next_access_vtime = req->next_access_vtime;
  obj->GLCache.last_access_rtime = params->curr_rtime;
  obj->GLCache.last_access_vtime = params->curr_vtime;
  obj->GLCache.freq += 1;

  segment_t *seg = obj->GLCache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];
}

/* some internal state update when an object is evicted */
static inline void obj_evict_update(cache_t *cache, cache_obj_t *obj) {
  GLCache_params_t *params = cache->eviction_params;
  segment_t *seg = obj->GLCache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];
}

/* calculate the score of object, the larger score,
   the more important object is, we should keep it */
static inline double cal_obj_score(GLCache_params_t *params,
                                   obj_score_type_e score_type,
                                   cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->GLCache.segment;
  int64_t curr_rtime = params->curr_rtime;
  int64_t curr_vtime = params->curr_vtime;
  bucket_t *bkt = &params->buckets[seg->bucket_id];
  double age_vtime =
      (double)(curr_vtime - cache_obj->GLCache.last_access_vtime);
  if (score_type == OBJ_SCORE_FREQ) {
    return (double)cache_obj->GLCache.freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    return (double)(cache_obj->GLCache.freq + 0.01) * 1.0e6 /
           cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_AGE_BYTE) {
    return 1.0e8 / (double)cache_obj->obj_size / age_vtime;

  } else if (score_type == OBJ_SCORE_FREQ_AGE_BYTE) {
    return (double)(cache_obj->GLCache.freq + 0.01) * 1.0e8 /
           cache_obj->obj_size / age_vtime;

  } else if (score_type == OBJ_SCORE_FREQ_AGE) {
    return (double)(cache_obj->GLCache.freq + 0.01) * 1.0e6 /
           (curr_rtime - cache_obj->GLCache.last_access_rtime);

  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->misc.next_access_vtime == -1 ||
        cache_obj->misc.next_access_vtime == INT64_MAX) {
      return 0;
    }

    DEBUG_ASSERT(cache_obj->misc.next_access_vtime > curr_vtime);
    return 1.0e8 / (double)cache_obj->obj_size /
           (double)(cache_obj->misc.next_access_vtime - curr_vtime);

  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

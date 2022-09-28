#pragma once

#include "../../include/libCacheSim/cacheObj.h"
#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "GLCacheInternal.h"

static inline void obj_init(cache_t *cache, const request_t *req, cache_obj_t *cache_obj,
                            segment_t *seg) {
  GLCache_params_t *params = cache->eviction_params;
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->GLCache.freq = 0;
  cache_obj->GLCache.last_access_rtime = req->real_time;
  cache_obj->GLCache.last_access_vtime = params->curr_vtime;
  cache_obj->GLCache.in_cache = 1;
  cache_obj->GLCache.seen_after_snapshot = 0;
  cache_obj->GLCache.next_access_vtime = req->next_access_vtime;

  cache_obj->GLCache.segment = seg;
  cache_obj->GLCache.idx_in_segment = seg->n_obj;
}

static inline int64_t obj_age(GLCache_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->GLCache.last_access_rtime;
}

#ifdef USE_LHD
static inline int64_t object_age_shifted(GLCache_params_t *params, cache_obj_t *obj) {
  bucket_t *bkt = &params->buckets[((segment_t *) (obj->GLCache.segment))->bucket_id];
  int64_t obj_age =
      (params->curr_rtime - obj->GLCache.last_access_rtime) >> bkt->hit_prob->age_shift;
  if (obj_age >= HIT_PROB_MAX_AGE) {
    bkt->hit_prob->n_overflow += 1;
    obj_age = HIT_PROB_MAX_AGE - 1;
  }
  return obj_age;
}

static inline int64_t hitAgeClass(int64_t age) {
  if (age == 0) {
    return HIT_PROB_CLASSES - 1;
  }
  int64_t log = 0;
  while (age < HIT_PROB_MAX_AGE && log < HIT_PROB_CLASSES - 1) {
    age <<= 1;
    log += 1;
  }
  return log;
}

static inline int64_t hitSizeClass(uint32_t size) {
  return ((uint64_t) log(size)) % HIT_PROB_CLASSES;
}
#endif 

/* some internal state update when an object is requested */
static inline void obj_hit_update(GLCache_params_t *params, cache_obj_t *obj, const request_t *req) {
  obj->GLCache.next_access_vtime = req->next_access_vtime;
  obj->GLCache.last_access_rtime = params->curr_rtime;
  obj->GLCache.last_access_vtime = params->curr_vtime;
  obj->GLCache.freq += 1;

  segment_t *seg = obj->GLCache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];

#ifdef USE_LHD
  int64_t obj_age_shifted = object_age_shifted(params, obj);
  bkt->hit_prob->n_hit[hitAgeClass(obj_age(params, obj))][obj_age_shifted] += 1;
  // bkt->hit_prob->n_hit[hitSizeClass(obj->obj_size)][obj_age_shifted] += 1;
#endif
}

/* some internal state update bwhen an object is evicted */
static inline void obj_evict_update(cache_t *cache, cache_obj_t *obj) {
  GLCache_params_t *params = cache->eviction_params;
  segment_t *seg = obj->GLCache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];

#ifdef USE_LHD
  int64_t obj_age_shifted = object_age_shifted(params, obj);
  bkt->hit_prob->n_evict[hitAgeClass(obj_age(params, obj))][obj_age_shifted] += 1;
  // bkt->hit_prob->n_evict[hitSizeClass(obj->obj_size)][obj_age_shifted] += 1;
#endif
}

/* calculate the score of object, the larger score, 
   the more important object is, we should keep it */
static inline double cal_obj_score(GLCache_params_t *params, obj_score_type_e score_type,
                                   cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->GLCache.segment;
  int64_t curr_rtime = params->curr_rtime; 
  int64_t curr_vtime = params->curr_vtime; 
  bucket_t *bkt = &params->buckets[seg->bucket_id];
#if AGE_SHIFT_FACTOR == 0
  double age_vtime = (double) (curr_vtime - cache_obj->GLCache.last_access_vtime);
#else
  double age_vtime = (double) (((curr_vtime - cache_obj->GLCache.last_access_vtime) >> AGE_SHIFT_FACTOR) + 1);
  assert(age_vtime != 0);
#endif
  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->GLCache.freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
#ifdef BYTE_MISS_RATIO
    return (double) (cache_obj->GLCache.freq + 0.01) * 1.0e6;
#else
    return (double) (cache_obj->GLCache.freq + 0.01) * 1.0e6 / cache_obj->obj_size;
#endif 

  } else if (score_type == OBJ_SCORE_AGE_BYTE) {
#ifdef BYTE_MISS_RATIO
    return 1.0e8 / age_vtime;
#else 
    return 1.0e8 / (double) cache_obj->obj_size / age_vtime;
#endif

  } else if (score_type == OBJ_SCORE_FREQ_AGE_BYTE) {
#ifdef BYTE_MISS_RATIO
    return (double) (cache_obj->GLCache.freq + 0.01) * 1.0e8 
           / age_vtime;    
#else
    return (double) (cache_obj->GLCache.freq + 0.01) * 1.0e8 / cache_obj->obj_size
           / age_vtime;
#endif

  } else if (score_type == OBJ_SCORE_FREQ_AGE) {
    return (double) (cache_obj->GLCache.freq + 0.01) * 1.0e6
           / (curr_rtime - cache_obj->GLCache.last_access_rtime);

#ifdef USE_LHD
  } else if (score_type == OBJ_SCORE_HIT_DENSITY) {
    int64_t obj_age_shifted = object_age_shifted(params, cache_obj);
    obj_age_shifted = obj_age_shifted >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age_shifted;

#ifdef BYTE_MISS_RATIO
    return 1.0e6 * bkt->hit_prob->hit_density[hitAgeClass(obj_age(params, cache_obj))][obj_age_shifted];
    // return 1.0e6 * bkt->hit_prob->hit_density[hitSizeClass(cache_obj->obj_size)][obj_age_shifted];
#else
    return 1.0e6 * bkt->hit_prob->hit_density[hitAgeClass(obj_age(params, cache_obj))][obj_age_shifted] / cache_obj->obj_size;
    // return 1.0e6 * bkt->hit_prob->hit_density[hitSizeClass(cache_obj->obj_size)][obj_age_shifted] / cache_obj->obj_size;
#endif
#endif 

  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->GLCache.next_access_vtime == -1 || cache_obj->GLCache.next_access_vtime == INT64_MAX) {
      return 0;
    }

    DEBUG_ASSERT(cache_obj->GLCache.next_access_vtime > curr_vtime);
#ifdef BYTE_MISS_RATIO
    return 1.0e8
           / (double) (cache_obj->GLCache.next_access_vtime - curr_vtime);
#else
    return 1.0e8 / (double) cache_obj->obj_size
           / (double) (cache_obj->GLCache.next_access_vtime - curr_vtime);
#endif

  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

#pragma once

#include "../../include/libCacheSim/cacheObj.h"
#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

static inline void obj_init(cache_t *cache, request_t *req, cache_obj_t *cache_obj,
                            segment_t *seg) {
  L2Cache_params_t *params = cache->eviction_params;
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->L2Cache.freq = 0;
  cache_obj->L2Cache.last_access_rtime = req->real_time;
  cache_obj->L2Cache.last_access_vtime = params->curr_vtime;
  cache_obj->L2Cache.in_cache = 1;
  cache_obj->L2Cache.seen_after_snapshot = 0;
  cache_obj->L2Cache.next_access_vtime = req->next_access_vtime;

  cache_obj->L2Cache.segment = seg;
  cache_obj->L2Cache.idx_in_segment = seg->n_obj;
}

static inline int64_t obj_age(L2Cache_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->L2Cache.last_access_rtime;
}

static inline int64_t object_age_shifted(L2Cache_params_t *params, cache_obj_t *obj) {
  bucket_t *bkt = &params->buckets[((segment_t *) (obj->L2Cache.segment))->bucket_id];
  int64_t obj_age =
      (params->curr_rtime - obj->L2Cache.last_access_rtime) >> bkt->hit_prob->age_shift;
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

/* some internal state update when an object is requested */
static inline void obj_hit_update(L2Cache_params_t *params, cache_obj_t *obj, request_t *req) {
  obj->L2Cache.next_access_vtime = req->next_access_vtime;
  obj->L2Cache.last_access_rtime = params->curr_rtime;
  obj->L2Cache.last_access_vtime = params->curr_vtime;
  obj->L2Cache.freq += 1;

  segment_t *seg = obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];

  int64_t obj_age_shifted = object_age_shifted(params, obj);
  bkt->hit_prob->n_hit[hitAgeClass(obj_age(params, obj))][obj_age_shifted] += 1;
  // bkt->hit_prob->n_hit[hitSizeClass(obj->obj_size)][obj_age_shifted] += 1;
}

/* some internal state update bwhen an object is evicted */
static inline void obj_evict_update(cache_t *cache, cache_obj_t *obj) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t *seg = obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_id];

#ifdef TRACK_EVICTION_AGE
  record_eviction_age(cache, (int) (params->curr_rtime - obj->last_access_rtime));
#endif

  int64_t obj_age_shifted = object_age_shifted(params, obj);
  bkt->hit_prob->n_evict[hitAgeClass(obj_age(params, obj))][obj_age_shifted] += 1;
  // bkt->hit_prob->n_evict[hitSizeClass(obj->obj_size)][obj_age_shifted] += 1;
}

/* calculate the score of object, the larger score, 
   the more important object is, we should keep it */
static inline double cal_obj_score(L2Cache_params_t *params, obj_score_type_e score_type,
                                   cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->L2Cache.segment;
  int64_t curr_rtime = params->curr_rtime; 
  int64_t curr_vtime = params->curr_vtime; 
  bucket_t *bkt = &params->buckets[seg->bucket_id];
  #if AGE_SHIFT_FACTOR == 0
  double age_vtime = (double) (curr_vtime - cache_obj->L2Cache.last_access_vtime);
  #else
  double age_vtime = (double) (((curr_vtime - cache_obj->L2Cache.last_access_vtime) >> AGE_SHIFT_FACTOR) + 1);
  assert(age_vtime != 0);
  #endif
  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->L2Cache.freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    #ifdef BYTE_MISS_RATIO
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e6;
    #else
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e6 / cache_obj->obj_size;
    #endif 

  } else if (score_type == OBJ_SCORE_SIZE_AGE) {
    #ifdef BYTE_MISS_RATIO
    return 1.0e8 / age_vtime;
    #else 
    return 1.0e8 / (double) cache_obj->obj_size
           / age_vtime;
    #endif

  } else if (score_type == OBJ_SCORE_FREQ_AGE_BYTE) {
    #ifdef BYTE_MISS_RATIO
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e8 
           / age_vtime;    
    #else
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e8 / cache_obj->obj_size
           / age_vtime;
    #endif

  } else if (score_type == OBJ_SCORE_FREQ_AGE) {
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e6
           / (curr_rtime - cache_obj->L2Cache.last_access_rtime);

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

  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->L2Cache.next_access_vtime == -1 || cache_obj->L2Cache.next_access_vtime == INT64_MAX) {
      return 0;
    }

    DEBUG_ASSERT(cache_obj->L2Cache.next_access_vtime > curr_vtime);
    #ifdef BYTE_MISS_RATIO
    return 1.0e8
           / (double) (cache_obj->L2Cache.next_access_vtime - curr_vtime);
    #else
    return 1.0e8 / (double) cache_obj->obj_size
           / (double) (cache_obj->L2Cache.next_access_vtime - curr_vtime);
    #endif

  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

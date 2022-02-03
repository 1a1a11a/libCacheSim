#pragma once

#include "../../include/libCacheSim/cacheObj.h"
#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

static inline void object_init(cache_t *cache, request_t *req, cache_obj_t *cache_obj,
                               segment_t *seg) {
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->L2Cache.freq = 0;
  cache_obj->L2Cache.last_access_rtime = req->real_time;
  cache_obj->L2Cache.in_cache = 1;
  cache_obj->L2Cache.seen_after_snapshot = 0;
  cache_obj->L2Cache.next_access_vtime = req->next_access_vtime;

  cache_obj->L2Cache.segment = seg;
  cache_obj->L2Cache.idx_in_segment = seg->n_obj;
}

static inline int64_t object_age(L2Cache_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->L2Cache.last_access_rtime;
}

static inline int64_t object_age_shifted(L2Cache_params_t *params, cache_obj_t *obj) {
  bucket_t *bkt = &params->buckets[((segment_t *) (obj->L2Cache.segment))->bucket_idx];
  int64_t obj_age =
      (params->curr_rtime - obj->L2Cache.last_access_rtime) >> bkt->hit_prob->age_shift;
  if (obj_age >= HIT_PROB_MAX_AGE) {
    bkt->hit_prob->n_overflow += 1;
    obj_age = HIT_PROB_MAX_AGE - 1;
  }
  return obj_age;
}

static inline void object_hit(L2Cache_params_t *params, cache_obj_t *obj, request_t *req) {
  obj->L2Cache.next_access_vtime = req->next_access_vtime;
  obj->L2Cache.last_access_rtime = params->curr_rtime;
  obj->L2Cache.freq += 1;

  segment_t *seg = obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  int64_t obj_age = object_age_shifted(params, obj);
  bkt->hit_prob->n_hit[obj_age] += 1;
}

static inline void object_evict(cache_t *cache, cache_obj_t *obj) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t *seg = obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

#ifdef TRACK_EVICTION_AGE
  record_eviction_age(cache, (int) (params->curr_rtime - obj->last_access_rtime));
#endif

  int64_t obj_age = object_age_shifted(params, obj);
  bkt->hit_prob->n_evict[obj_age] += 1;
}

/* calculate the score of object, the larger score, 
   the more important object is, we should keep it */
static inline double cal_object_score(L2Cache_params_t *params, obj_score_type_e score_type,
                                      cache_obj_t *cache_obj, int curr_rtime,
                                      int64_t curr_vtime) {
  segment_t *seg = cache_obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->L2Cache.freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e6 / cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_FREQ_AGE_BYTE) {
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e8 / cache_obj->obj_size
           / (curr_rtime - cache_obj->L2Cache.last_access_rtime);

  } else if (score_type == OBJ_SCORE_FREQ_AGE) {
    return (double) (cache_obj->L2Cache.freq + 0.01) * 1.0e6
           / (curr_rtime - cache_obj->L2Cache.last_access_rtime);

  } else if (score_type == OBJ_SCORE_HIT_DENSITY) {
    int64_t obj_age = object_age(params, cache_obj);
    obj_age = obj_age >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age;
    return 1.0e6 * bkt->hit_prob->hit_density[obj_age] / cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->L2Cache.next_access_vtime == -1) {
      return 0;
    }

    DEBUG_ASSERT(cache_obj->L2Cache.next_access_vtime > curr_vtime);
    return 1.0e8 / (double) cache_obj->obj_size
           / (double) (cache_obj->L2Cache.next_access_vtime - curr_vtime);

  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

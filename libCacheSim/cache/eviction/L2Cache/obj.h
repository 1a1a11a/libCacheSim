#pragma once 

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "../../include/libCacheSim/cacheObj.h"
#include "L2CacheInternal.h"


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
  obj->L2Cache.freq += 1;

  segment_t *seg = obj->L2Cache.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  int64_t obj_age = object_age_shifted(params, obj);
  bkt->hit_prob->n_hit[obj_age] += 1; 
  obj->L2Cache.last_access_rtime = params->curr_rtime;
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


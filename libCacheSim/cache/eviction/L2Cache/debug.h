#pragma once 

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "../../include/libCacheSim/cache.h"
#include "L2CacheInternal.h"


static inline void _debug_check_bucket_segs(bucket_t *bkt) {
  segment_t *curr_seg = bkt->first_seg;
  int n_seg = 0;
  segment_t *prev_seg = NULL;

  while (curr_seg) {
    n_seg += 1;
    assert(curr_seg->prev_seg == prev_seg);
    assert(curr_seg->bucket_idx == bkt->bucket_idx);
    prev_seg = curr_seg;
    curr_seg = curr_seg->next_seg;
  }

  assert(prev_seg == bkt->last_seg);
  assert(n_seg == bkt->n_seg);
}

static inline void debug_check_bucket(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  segment_t *curr_seg;
  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      n_seg += 1;
      DEBUG_ASSERT(curr_seg != NULL);
      curr_seg = curr_seg->next_seg;
    }
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_segs == n_seg);

  n_seg = 0;
  curr_seg = params->training_bucket.first_seg;
  for (int si = 0; si < params->training_bucket.n_seg; si++) {
    n_seg += 1;
    DEBUG_ASSERT(curr_seg != NULL);
    curr_seg = curr_seg->next_seg;
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_training_segs == n_seg);
}



static inline int _debug_count_n_obj(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  int64_t n_obj = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    segment_t *curr_seg = params->buckets[i].first_seg;
    int n_seg_in_bucket = 0;
    while (curr_seg) {
      if (curr_seg->n_total_obj != params->segment_size) {
        printf("find segment not full, bucket %d, seg %d: %d objects training %d\n", i,
               n_seg_in_bucket, curr_seg->n_total_obj, curr_seg->is_training_seg);
      }
      n_obj += curr_seg->n_total_obj;
      curr_seg = curr_seg->next_seg;
      n_seg_in_bucket++;
    }
  }

  return n_obj;
}



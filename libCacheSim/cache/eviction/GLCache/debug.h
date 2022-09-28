#pragma once 

#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "../../include/libCacheSim/cache.h"
#include "GLCacheInternal.h"


static inline void _debug_check_bucket_segs(bucket_t *bkt) {
  segment_t *curr_seg = bkt->first_seg;
  int n_in_use_segs = 0;
  segment_t *prev_seg = NULL;

  while (curr_seg) {
    n_in_use_segs += 1;
    assert(curr_seg->prev_seg == prev_seg);
    assert(curr_seg->bucket_id == bkt->bucket_id);
    prev_seg = curr_seg;
    curr_seg = curr_seg->next_seg;
  }

  assert(prev_seg == bkt->last_seg);
  assert(n_in_use_segs == bkt->n_in_use_segs);
}

static inline void debug_check_bucket(cache_t *cache) {
  GLCache_params_t *params = (GLCache_params_t *) cache->eviction_params;

  segment_t *curr_seg;
  int n_in_use_segs = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
      n_in_use_segs += 1;
      DEBUG_ASSERT(curr_seg != NULL);
      curr_seg = curr_seg->next_seg;
    }
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_in_use_segs == n_in_use_segs);

  n_in_use_segs = 0;
  curr_seg = params->train_bucket.first_seg;
  for (int si = 0; si < params->train_bucket.n_in_use_segs; si++) {
    n_in_use_segs += 1;
    DEBUG_ASSERT(curr_seg != NULL);
    curr_seg = curr_seg->next_seg;
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_training_segs == n_in_use_segs);
}



static inline int _debug_count_n_obj(cache_t *cache) {
  GLCache_params_t *params = (GLCache_params_t *) cache->eviction_params;
  int64_t n_obj = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    segment_t *curr_seg = params->buckets[i].first_seg;
    int n_seg_in_bucket = 0;
    while (curr_seg) {
      if (curr_seg->n_obj != params->segment_size) {
        printf("find segment not full, bucket %d, seg %d: %d objects training %d\n", i,
               n_seg_in_bucket, curr_seg->n_obj, curr_seg->selected_for_training);
      }
      n_obj += curr_seg->n_obj;
      curr_seg = curr_seg->next_seg;
      n_seg_in_bucket++;
    }
  }

  return n_obj;
}


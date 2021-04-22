#pragma once


#include "../../include/libCacheSim/evictionAlgo/LLSC.h"
#include "log.h"


static inline void transform_seg_to_training(cache_t *cache, bucket_t *bucket, segment_t *segment) {
  LLSC_params_t *params = cache->cache_params;
  segment->is_training_seg = true;

  /* remove from the bucket */
  remove_seg_from_bucket(bucket, segment);

  /* add to training bucket */
  params->n_segs -= 1;
  params->n_training_segs += 1;

  bucket->n_seg -= 1;
  params->training_bucket.n_seg += 1;

  append_seg_to_bucket(&params->training_bucket, segment);
}

static inline int seg_history_idx(segment_t *segment, int32_t curr_time) {

  return 0;
}

static inline void seg_hit(segment_t *segment, int last_history_idx) {
  ;
}


void clean_training_segs(cache_t *cache);

void training();


void inference();



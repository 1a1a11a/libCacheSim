#pragma once

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"
#include "log.h"

static inline void transform_seg_to_training(cache_t *cache, bucket_t *bucket,
                                             segment_t *segment) {
  LLSC_params_t *params = cache->cache_params;
  segment->is_training_seg = true;
  segment->eviction_vtime = params->curr_vtime;

  /* remove from the bucket */
  remove_seg_from_bucket(params, bucket, segment);

  append_seg_to_bucket(params, &params->training_bucket, segment);
}

static inline int seg_history_idx(segment_t *segment, int32_t curr_time, int32_t time_window) {
  int idx = (curr_time - segment->create_rtime) / time_window;

  if (idx >= N_FEATURE_TIME_WINDOW) return N_FEATURE_TIME_WINDOW - 1;
  else
    return idx;
}

static inline void seg_hit(LLSC_params_t *params, cache_obj_t *cache_obj) {
  if (!params->learner.start_feature_recording) return;

  segment_t *segment = cache_obj->LSC.segment;
  segment->n_total_hit += 1;
  int last_history_idx = (int) cache_obj->LSC.last_history_idx;
  int curr_idx =
      seg_history_idx(segment, params->curr_rtime, params->learner.feature_history_time_window);
  DEBUG_ASSERT(curr_idx >= last_history_idx);

  if (curr_idx > last_history_idx) {
    segment->feature.n_active_item_per_window[curr_idx] += 1;
    segment->feature.n_active_byte_per_window[curr_idx] += cache_obj->obj_size;
  }

  if (last_history_idx == -1) {
    segment->n_total_active += 1;
    segment->feature.n_active_item_accu[curr_idx] += 1;
    segment->feature.n_active_byte_accu[curr_idx] += cache_obj->obj_size;
  }

  segment->feature.n_hit[curr_idx] += 1;
  cache_obj->LSC.last_history_idx = curr_idx;
  //  cache_obj->LSC.last_access_rtime = params->curr_rtime;
}

void train(cache_t *cache);

void inference(cache_t *cache);

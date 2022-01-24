#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "log.h"


static inline void transform_seg_to_training(cache_t *cache, bucket_t *bucket,
                                             segment_t *segment) {
  static int n = 0, n_zero = 0;
  L2Cache_params_t *params = cache->eviction_params;
  segment->is_training_seg = true;
  /* used to calculate the eviction penalty */
  segment->become_train_seg_vtime = params->curr_vtime;
  /* used to calculate age at eviction for training */
  segment->become_train_seg_rtime = params->curr_rtime;
  segment->penalty = 0;


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

static inline void seg_feature_shift(L2Cache_params_t *params, segment_t *seg) {

  int64_t shift;


  shift = (params->curr_rtime - seg->feature.last_min_window_ts) / 60;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW-1; i >= 0; i--) {
    seg->feature.n_hit_per_min[i + 1] = seg->feature.n_hit_per_min[i];
  }
  seg->feature.n_hit_per_min[0] = 0;
  seg->feature.last_min_window_ts = params->curr_rtime;

  shift = (params->curr_rtime - seg->feature.last_ten_min_window_ts) / 600;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW-1; i >= 0; i--) {
    seg->feature.n_hit_per_ten_min[i + 1] = seg->feature.n_hit_per_ten_min[i];
  }
  seg->feature.n_hit_per_ten_min[0] = 0;
  seg->feature.last_ten_min_window_ts = params->curr_rtime;


  shift = (params->curr_rtime - seg->feature.last_hour_window_ts) / 3600;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW-1; i >= 0; i--) {
    seg->feature.n_hit_per_hour[i + 1] = seg->feature.n_hit_per_hour[i];
  }
  seg->feature.n_hit_per_hour[0] = 0;
  seg->feature.last_hour_window_ts = params->curr_rtime;
}

static inline void seg_hit(L2Cache_params_t *params, cache_obj_t *cache_obj) {
  //  if (!params->learner.start_feature_recording) return;

  segment_t *segment = cache_obj->L2Cache.segment;
  segment->n_total_hit += 1;

  if (params->curr_rtime - segment->feature.last_min_window_ts >= 60) {
    seg_feature_shift(params, segment);
  }

  segment->feature.n_hit_per_min[0] += 1;
  segment->feature.n_hit_per_ten_min[0] += 1;
  segment->feature.n_hit_per_hour[0] += 1;

  if (!cache_obj->L2Cache.active) {
    segment->n_total_active += 1;
    cache_obj->L2Cache.active = 1;
  }
}

static inline void update_train_y(L2Cache_params_t *params, cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->L2Cache.segment;

#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_EVICTION
  DEBUG_ASSERT(cache_obj->L2Cache.in_cache == 0);
  DEBUG_ASSERT(seg->is_training_seg == true);
#else
  if (!seg->in_training_data) {
    return;
  }
#endif

#if TRAINING_CONSIDER_RETAIN == 1
  if (seg->n_skipped_penalty ++ > params->n_retain_from_seg)
#endif
  {
    double age = (double) params->curr_vtime - seg->become_train_seg_vtime;
    if (params->obj_score_type == OBJ_SCORE_FREQ
        || params->obj_score_type == OBJ_SCORE_FREQ_AGE) {
      seg->penalty += 1.0e8 / age;
    } else {
      seg->penalty += 1.0e8 / age / cache_obj->obj_size;
    }
    params->learner.training_y[seg->training_data_row_idx] = seg->penalty;
  }
}

void create_data_holder2(cache_t *cache);

void snapshot_segs_to_training_data(cache_t *cache);

void train(cache_t *cache);

void inference(cache_t *cache);

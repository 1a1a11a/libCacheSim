
#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "bucket.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"

void seg_hit(L2Cache_params_t *params, cache_obj_t *cache_obj) {

  segment_t *segment = cache_obj->L2Cache.segment;
  segment->n_hit += 1;

  if (params->curr_rtime - segment->feature.last_min_window_ts >= 60) {
    seg_feature_shift(params, segment);
  }

  segment->feature.n_hit_per_min[0] += 1;
  segment->feature.n_hit_per_ten_min[0] += 1;
  segment->feature.n_hit_per_hour[0] += 1;

  if (!cache_obj->L2Cache.active) {
    segment->n_active += 1;
    cache_obj->L2Cache.active = 1;
  }
}

/* because we use windowed feature, we need to shift the feature
 * when the window moves */
void seg_feature_shift(L2Cache_params_t *params, segment_t *seg) {

  int64_t shift;

  /* whether it is a new min window now */
  shift = (params->curr_rtime - seg->feature.last_min_window_ts) / 60;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW - 1; i >= 0; i--) {
    seg->feature.n_hit_per_min[i + 1] = seg->feature.n_hit_per_min[i];
  }
  seg->feature.n_hit_per_min[0] = 0;
  seg->feature.last_min_window_ts = params->curr_rtime;

  /* whether it is a new 10-min window now */
  shift = (params->curr_rtime - seg->feature.last_ten_min_window_ts) / 600;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW - 1; i >= 0; i--) {
    seg->feature.n_hit_per_ten_min[i + 1] = seg->feature.n_hit_per_ten_min[i];
  }
  seg->feature.n_hit_per_ten_min[0] = 0;
  seg->feature.last_ten_min_window_ts = params->curr_rtime;

  /* whether it is a new hour window now */
  shift = (params->curr_rtime - seg->feature.last_hour_window_ts) / 3600;
  if (shift <= 0) return;
  for (int i = N_FEATURE_TIME_WINDOW - 1; i >= 0; i--) {
    seg->feature.n_hit_per_hour[i + 1] = seg->feature.n_hit_per_hour[i];
  }
  seg->feature.n_hit_per_hour[0] = 0;
  seg->feature.last_hour_window_ts = params->curr_rtime;
}

/** 
 * @brief update the segment feature when a segment is evicted
 * 
 * features: 
 *    bucket_id: the bucket id of the segment
 *    create_hour: the hour when the segment is created
 *    create_min: the min when the segment is created
 *    age: the age of the segment
 *    req_rate: the request rate of the segment when it was created
 *    write_rate: the write rate of the segment when it was created
 *    mean_obj_size 
 *    miss_ratio: the miss ratio of the segment when it was created
 *    NA
 *    n_hit: the number of requests so far 
 *    n_active: the number of active objects 
 *    n_merge: the number of times it has been merged 
 *    n_hit_per_min: the number of requests per min
 *    n_hit_per_ten_min: the number of requests per 10 min
 *    n_hit_per_hour: the number of requests per hour
 * 
 * @param is_training_data: whether the data is training or inference 
 * @param x: feature vector, and we write to x 
 * @param y: label, for passing the true y 
 * 
 */
bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool is_training_data, feature_t *x,
                     train_y_t *y) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  // debug
  //  x[0] = x[1] = x[2] = x[3] = x[4] = x[5] = x[6] = x[7] = x[8] = x[9] = x[10] = x[11] = 0;

  x[0] = (feature_t) curr_seg->bucket_id;
  x[1] = (feature_t) ((curr_seg->create_rtime / 3600) % 24);
  x[2] = (feature_t) ((curr_seg->create_rtime / 60) % 60);
  if (is_training_data) {
    x[3] = (feature_t) curr_seg->become_train_seg_rtime - curr_seg->create_rtime;
    assert(curr_seg->become_train_seg_rtime == params->curr_rtime);
  } else {
    x[3] = (feature_t) params->curr_rtime - curr_seg->create_rtime;
  }
  x[4] = (feature_t) curr_seg->req_rate;
  x[5] = (feature_t) curr_seg->write_rate;
  x[6] = (feature_t) curr_seg->n_byte / curr_seg->n_obj;
  x[7] = (feature_t) curr_seg->miss_ratio;
  x[8] = 0.0;
  x[9] = curr_seg->n_hit;
  x[10] = curr_seg->n_active;
  x[11] = curr_seg->n_merge;

  for (int k = 0; k < N_FEATURE_TIME_WINDOW; k++) {
    x[12 + k * 3 + 0] = (feature_t) curr_seg->feature.n_hit_per_min[k];
    x[12 + k * 3 + 1] = (feature_t) curr_seg->feature.n_hit_per_ten_min[k];
    x[12 + k * 3 + 2] = (feature_t) curr_seg->feature.n_hit_per_hour[k];

    //    x[12 + k * 3 + 0] = 0;
    //    x[12 + k * 3 + 1] = 0;
    //    x[12 + k * 3 + 2] = 0;
  }

  // if (is_training_data)
  //   printf("train: ");
  // else
  //   printf("test:  ");

  // printf("%.0f/%.0f/%.0f/%.0f | %.0f, %.0f, %.0f, %.4f | %.0f/%.0f/%.0f, %.0f|%.0f|%.0f\n",
  //   x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[9], x[10], x[11], x[12], x[20], x[28]
  // );

  // for (int j = 0; j < 36; j++) {
  //   printf("%.2f, ", x[j]);
  // }
  // printf("\n");

  if (y == NULL) {
    // this is for inference
    return true;
  }

  /* calculate y for training */
  double online_utility = curr_seg->train_utility;
  double offline_utility = -1;
  *y = (train_y_t) online_utility;
  int n_retained_obj = 0;

#if TRAINING_CONSIDER_RETAIN == 1
  n_retained_obj = params->n_retain_per_seg;
#endif

  if (params->train_source_y == TRAIN_Y_FROM_ORACLE) {
    /* lower utility should be evicted first */
    offline_utility = cal_seg_utility_oracle(cache, curr_seg, curr_seg->become_train_seg_rtime, curr_seg->become_train_seg_vtime);
    *y = (train_y_t) offline_utility;
  }

  return *y > 0.000001;
}

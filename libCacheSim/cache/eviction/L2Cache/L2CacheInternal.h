#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "const.h"

typedef struct {
  /* rolling stat on hits, 
   * number of hits in the N_FEATURE_TIME_WINDOW min, 10min, hour */
  int32_t n_hit_per_min[N_FEATURE_TIME_WINDOW];
  int32_t n_hit_per_ten_min[N_FEATURE_TIME_WINDOW];
  int32_t n_hit_per_hour[N_FEATURE_TIME_WINDOW];

  /* used to calculate when the next window starts */
  int64_t last_min_window_ts;
  int64_t last_ten_min_window_ts;
  int64_t last_hour_window_ts;
} seg_feature_t;

/* LHD related states */
typedef struct hitProb {
  int32_t n_hit[HIT_PROB_MAX_AGE];
  int32_t n_evict[HIT_PROB_MAX_AGE];

  double hit_density[HIT_PROB_MAX_AGE];
  int64_t n_overflow;
  int32_t age_shift;
} hitProb_t;

typedef struct learner {
  int64_t last_train_rtime;
  int retrain_intvl;

#ifdef USE_XGBOOST
  BoosterHandle booster; // model
  DMatrixHandle train_dm;// training data
  DMatrixHandle valid_dm;// validation data
  DMatrixHandle inf_dm;  // inference data
#elif defined(USE_GBM)
  DatasetHandle train_dm;// training data
#endif

  /* learner stat */
  int n_train;
  int n_inference;
  int n_evictions_last_hour;

  feature_t *train_x;
  train_y_t *train_y;
  train_y_t *train_y_oracle;
  feature_t *valid_x;
  train_y_t *valid_y;
  feature_t *inference_x;
  pred_t *pred;

  int n_feature;
  unsigned int n_train_samples;
  unsigned int n_valid_samples;
  int n_trees;

  int32_t train_matrix_n_row; /* the size of matrix */
  int32_t valid_matrix_n_row;
  int32_t inf_matrix_n_row;

} learner_t;

typedef struct cache_state {
  double write_rate;
  double req_rate;
  double miss_ratio;
  // double        cold_miss_ratio;

  int64_t last_update_rtime;
  int64_t last_update_vtime;

  int64_t n_miss;
  int64_t n_miss_bytes;

  int64_t n_evicted_bytes;
} cache_state_t;

typedef struct segment {

  cache_obj_t *objs;

  int32_t seg_id;
  int32_t bucket_id;
  // used to connect segments in the same bucket in create order
  struct segment *prev_seg;
  struct segment *next_seg;

  // used to connect segments in the same bucket in ranked order
  struct segment *next_ranked_seg;

  double pred_utility;
  double train_utility;
  int n_skipped_penalty;
  int rank;

  /* stat when segment is cached */
  int64_t n_byte;
  int32_t n_obj;
  int32_t n_hit;
  int32_t n_active;
  int n_merge; /* number of merged times */

  /* cache stat when segment was created */
  int32_t create_rtime;
  int64_t create_vtime;
  double req_rate;
  double write_rate;
  double miss_ratio;

  /* training related */
  bool selected_for_training;// whether this segment has been chosen to be training data
  int64_t become_train_seg_vtime;
  int64_t become_train_seg_rtime;
  unsigned int training_data_row_idx;

  seg_feature_t feature;

  int32_t magic;
} segment_t;

/* bucket */
typedef struct {
  int64_t bucket_property;
  segment_t *first_seg;
  segment_t *last_seg;
  segment_t *ranked_seg_head;
  segment_t *ranked_seg_tail;

  int32_t n_in_use_segs;
  int16_t bucket_id;
  segment_t *next_seg_to_evict;
  hitProb_t *hit_prob;// TODO: move to LHD
} bucket_t;

typedef struct obj_sel {
  segment_t **segs_to_evict;

  double *score_array;
  int score_array_size;
} obj_sel_t;

/* parameters and state related to segment selection */
typedef struct seg_sel {
  int64_t last_rank_time;// in number of evictions
  segment_t **ranked_segs;

  int32_t n_ranked_segs;  // the number of ranked segments (ranked_segs.size())
  int32_t ranked_seg_size;// the malloc-ed size of ranked_segs (ranked_segs.capacity())
  int32_t ranked_seg_pos; // the position of the next segment to be evicted

  // TODO: per bucket ranked segs
} seg_sel_t;

/* parameters and state related to cache */
typedef struct {
  int segment_size; /* in temrs of number of objects */
  int n_merge;
  /* retain n objects from each seg, total retain n_retain * n_merge objects */
  int n_retain_per_seg;
  // whether we merge consecutive segments (with the first segment has the lowest utility)
  // or we merge non-consecutive segments based on ranking
  bool merge_consecutive_segs;
  train_source_e train_source_x;
  train_source_e train_source_y;

  // cache state
  int32_t n_in_use_segs;
  int n_used_buckets;
  bucket_t buckets[MAX_N_BUCKET];
  bucket_t train_bucket; /* segments that are evicted and used for training */
  int curr_evict_bucket_idx;

  int64_t start_rtime;
  int64_t curr_rtime;
  int64_t curr_vtime;

  // current number of ghost segments for training
  int32_t n_training_segs;

  int64_t n_allocated_segs;
  int64_t n_evictions;

  seg_sel_t seg_sel;

  obj_sel_t obj_sel; /* for selecting objects to evict */

  learner_t learner;

  cache_state_t cache_state;

  /* cache type */
  L2Cache_type_e type;

  /* bucket and related parameters */
  bucket_type_e bucket_type;

  /* object selection related parameters */
  obj_score_type_e obj_score_type;

  //  int64_t last_hit_prob_compute_rtime;
  int64_t last_hit_prob_compute_vtime; /* LHD selection */

  /* in number of evictions */
  double rank_intvl;
} L2Cache_params_t;

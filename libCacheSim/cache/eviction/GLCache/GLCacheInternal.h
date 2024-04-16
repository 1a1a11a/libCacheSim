#pragma once

#include <xgboost/c_api.h>

#include "../../../include/libCacheSim/cache.h"
#include "const.h"

typedef float feature_t;
typedef float pred_t;
typedef float train_y_t;

typedef enum {
  LOGCACHE_TWO_ORACLE = 1,   // oracle to select group and object
  LOGCACHE_LOG_ORACLE = 2,   // oracle to select group, obj_score to select obj
  LOGCACHE_ITEM_ORACLE = 3,  // FIFO for seg selection, oracle for obj selection
  LOGCACHE_LEARNED = 4,
} GLCache_type_e;

typedef enum obj_score_type {
  OBJ_SCORE_FREQ = 0,
  OBJ_SCORE_FREQ_BYTE = 1,
  OBJ_SCORE_FREQ_AGE = 2,
  OBJ_SCORE_FREQ_AGE_BYTE = 3,
  OBJ_SCORE_AGE_BYTE = 6,

  OBJ_SCORE_ORACLE = 8,
} obj_score_type_e;

typedef enum training_source {
  TRAIN_Y_FROM_ONLINE,
  TRAIN_Y_FROM_ORACLE,
} train_source_e;

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

typedef struct learner {
  int64_t last_train_rtime;

  BoosterHandle booster;   // model
  DMatrixHandle train_dm;  // training data
  DMatrixHandle valid_dm;  // validation data
  DMatrixHandle inf_dm;    // inference data

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
  int n_seg_util_skipped;
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
  bool selected_for_training;  // whether this segment has been chosen to be
                               // training data
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
} bucket_t;

typedef struct double_double_pair {
  double x;
  double y;
} dd_pair_t;

typedef struct obj_sel {
  segment_t **segs_to_evict;

  double *score_array;
#ifdef RANDOMIZE_MERGE
  double *score_array_offset;
#endif
  dd_pair_t *dd_pair_array;
  int array_size;
} obj_sel_t;

/* parameters and state related to segment selection */
typedef struct seg_sel {
  segment_t **ranked_segs;

  int32_t n_ranked_segs;  // the number of ranked segments (ranked_segs.size())
  int32_t ranked_seg_size;  // the malloc-ed size of ranked_segs
                            // (ranked_segs.capacity())
  int32_t ranked_seg_pos;   // the position of the next segment to be evicted
} seg_sel_t;

/* parameters and state related to cache */
typedef struct {
  /* user parameters */
  int segment_size; /* in terms of number of objects */
  int n_merge;
  // whether we merge consecutive segments (with the first segment has the
  // lowest utility) or we merge non-consecutive segments based on ranking
  bool merge_consecutive_segs;
  int retrain_intvl;
  train_source_e train_source_y;
  GLCache_type_e type;
  double rank_intvl;

  /* calculated parameters */
  /* retain n objects from each seg, total retain n_retain * n_merge objects */
  int n_retain_per_seg;
  /* object selection related parameters */
  obj_score_type_e obj_score_type;

  // cache state
  int32_t n_in_use_segs;
  int n_used_buckets;
  bucket_t buckets[MAX_N_BUCKET];
  bucket_t train_bucket; /* segments that are evicted and used for training */
  int curr_evict_bucket_idx;

  int64_t start_rtime;
  /* rtime is wall clock time */
  int64_t curr_rtime;
  /* vtime is reference count*/
  int64_t curr_vtime;

  // current number of ghost segments for training
  int32_t n_training_segs;

  int64_t n_allocated_segs;
  int64_t n_evictions;

  seg_sel_t seg_sel;

  obj_sel_t obj_sel; /* for selecting objects to evict */

  learner_t learner;

  cache_state_t cache_state;

} GLCache_params_t;

/********************** init ********************/
void init_global_params();
void deinit_global_params();
void check_params(GLCache_params_t *init_params);
void init_seg_sel(cache_t *cache);
void init_obj_sel(cache_t *cache);
void init_learner(cache_t *cache);
void init_cache_state(cache_t *cache);

/********************** bucket ********************/
/* append a segment to the end of bucket */
void append_seg_to_bucket(GLCache_params_t *params, bucket_t *bucket,
                          segment_t *segment);

void remove_seg_from_bucket(GLCache_params_t *params, bucket_t *bucket,
                            segment_t *segment);

void print_bucket(cache_t *cache);

/********************** segment ********************/
segment_t *allocate_new_seg(cache_t *cache, int bucket_id);

void link_new_seg_before_seg(GLCache_params_t *params, bucket_t *bucket,
                             segment_t *old_seg, segment_t *new_seg);

double find_cutoff(cache_t *cache, obj_score_type_e obj_score_type,
                   segment_t **segs, int n_segs, int n_retain);

double cal_seg_utility(cache_t *cache, segment_t *seg, bool oracle_obj_sel);

int clean_one_seg(cache_t *cache, segment_t *seg);

void print_seg(cache_t *cache, segment_t *seg, int log_level);

/********************** seg sel ********************/
bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs);

bucket_t *select_segs_weighted_fifo(cache_t *cache, segment_t **segs);

bucket_t *select_segs_rand(cache_t *cache, segment_t **segs);

bucket_t *select_segs_learned(cache_t *cache, segment_t **segs);

bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs);

void rank_segs(cache_t *cache);

/********************** eviction ********************/
bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs);

void GLCache_merge_segs(cache_t *cache, bucket_t *bucket, segment_t **segs);

int evict_one_seg(cache_t *cache, segment_t *seg);

/************* feature *****************/
void seg_hit_update(GLCache_params_t *params, cache_obj_t *cache_obj);

/************* learning *****************/
void train(cache_t *cache);

void inference(cache_t *cache);

/************* data preparation *****************/
void snapshot_segs_to_training_data(cache_t *cache);

void update_train_y(GLCache_params_t *params, cache_obj_t *cache_obj);

void prepare_training_data(cache_t *cache);

bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool training_data,
                     feature_t *x, train_y_t *y);

/********************** helper ********************/
#define safe_call(call)                                                      \
  {                                                                          \
    int err = (call);                                                        \
    if (err != 0) {                                                          \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, \
              XGBGetLastError());                                            \
      exit(1);                                                               \
    }                                                                        \
  }

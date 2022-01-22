//
//  L2Cache.h
//  libCacheSim
//

#pragma once

#include "../cache.h"


#define USE_XGBOOST

#ifdef USE_XGBOOST
#include <xgboost/c_api.h>
//#include <LightGBM/c_api.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_N_BUCKET 120
#define N_TRAIN_ITER 20
#define N_MAX_VALIDATION 1000
#define CACHE_STATE_UPDATE_INTVL 10

/* change to 10 will speed up */
#define N_FEATURE_TIME_WINDOW 8

#define TRAINING_DATA_FROM_EVICTION 1
#define TRAINING_DATA_FROM_CACHE    2
#define TRAINING_DATA_SOURCE    TRAINING_DATA_FROM_CACHE


#define TRAINING_TRUTH_ORACLE   1
#define TRAINING_TRUTH_ONLINE   2
//#define TRAINING_TRUTH    TRAINING_TRUTH_ONLINE
#define TRAINING_TRUTH    TRAINING_TRUTH_ORACLE
#define TRAINING_CONSIDER_RETAIN    1

#define REG 1
#define LTR 2
#define OBJECTIVE LTR

//#define ACCUMULATIVE_TRAINING
//#define TRAIN_ONCE

#define ADAPTIVE_RANK  1

/*********** exp *************/
//#define dump_ranked_seg_frac 0.05
//#undef ADAPTIVE_RANK


//#define DUMP_MODEL 1
//#define DUMP_TRAINING_DATA

#define HIT_PROB_MAX_AGE 86400
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
#define LHD_EWMA 0.9

#define MAGIC 1234567890

#define DEFAULT_RANK_INTVL 20

#ifdef USE_XGBOOST
typedef float feature_t;
typedef float pred_t;
typedef float train_y_t;
#elif defined(USE_GBM)
typedef double feature_t;
typedef double pred_t;
typedef float train_y_t;
#else
typedef double feature_t;
typedef double pred_t;
typedef double train_y_t;
#endif

typedef enum {
  SEGCACHE = 0,
  SEGCACHE_ITEM_ORACLE = 1,
  SEGCACHE_SEG_ORACLE = 2,
  SEGCACHE_BOTH_ORACLE = 3,

  LOGCACHE_START_POS = 4,

  LOGCACHE_BOTH_ORACLE = 5,
  LOGCACHE_LOG_ORACLE = 6,
  LOGCACHE_ITEM_ORACLE = 7,
  LOGCACHE_LEARNED = 8,
  //  LOGCACHE_RAMCLOUD,
  //  LOGCACHE_FIFO,
} LSC_type_e;

typedef enum obj_score_type {
  OBJ_SCORE_FREQ = 0,
  OBJ_SCORE_FREQ_BYTE = 1,
  OBJ_SCORE_FREQ_AGE = 2,

  OBJ_SCORE_HIT_DENSITY = 3,

  OBJ_SCORE_ORACLE = 4,
} obj_score_e;

typedef enum bucket_type {
  NO_BUCKET = 0,

  SIZE_BUCKET = 1,
  TTL_BUCKET = 2,
  CUSTOMER_BUCKET = 3,
  BUCKET_ID_BUCKET = 4,
  CONTENT_TYPE_BUCKET = 5,
} bucket_type_e;

typedef struct {
  int segment_size;
  int n_merge;
  LSC_type_e type;
  int size_bucket_base;
  int rank_intvl;
  int min_start_train_seg;
  int max_start_train_seg;
  int n_train_seg_growth;
  int sample_every_n_seg_for_training;
  int snapshot_intvl;
  int re_train_intvl;
  int hit_density_age_shift;
  bucket_type_e bucket_type;
} L2Cache_init_params_t;


typedef struct {
  int32_t n_hit_per_min[N_FEATURE_TIME_WINDOW];
  int32_t n_hit_per_ten_min[N_FEATURE_TIME_WINDOW];
  int32_t n_hit_per_hour[N_FEATURE_TIME_WINDOW];

  int64_t last_min_window_ts;
  int64_t last_ten_min_window_ts;
  int64_t last_hour_window_ts;
} seg_feature_t;

typedef struct learner {

  int64_t n_evicted_bytes;
//  int64_t last_train_vtime;
  int64_t last_train_rtime;
  int re_train_intvl;

#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_EVICTION
  int sample_every_n_seg_for_training;
  int n_segs_to_start_training;
  int64_t n_bytes_start_collect_train;    /* after evicting n_bytes_start_collect_train bytes,
                                           * start collecting training data */
#elif TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_CACHE
  int64_t last_snapshot_rtime;
  int32_t n_max_training_segs;
  int n_snapshot;
  int snapshot_intvl;
#else
#error
#endif


#ifdef USE_XGBOOST
  BoosterHandle booster;
  DMatrixHandle train_dm;
  DMatrixHandle valid_dm;
  DMatrixHandle inf_dm;
#elif defined(USE_GBM)
  DatasetHandle train_dm;

#endif

  int n_train;
  int n_inference;

  int n_feature;

  feature_t *training_x;
  train_y_t *training_y;
  feature_t *valid_x;
  train_y_t *valid_y;
//  pred_t *valid_pred_y;


  unsigned int n_training_samples;
  unsigned int n_validation_samples;
  unsigned int n_zero_samples;
  unsigned int n_zero_samples_use;
  int n_trees;

  int32_t train_matrix_size_row;   /* the size of matrix */
  int32_t valid_matrix_size_row;
  int32_t inf_matrix_size_row;

  feature_t *inference_data;
  pred_t *pred;

  int n_evictions_last_hour;
} learner_t;

typedef struct cache_state {
  double write_rate;
  double req_rate;
  double write_ratio;
  double cold_miss_ratio;

  int64_t last_update_rtime;
  int64_t last_update_vtime;

  int64_t n_miss;
} cache_state_t;

typedef struct segment {
  cache_obj_t *objs;
  int64_t total_bytes;
  int32_t n_total_obj;
  int32_t bucket_idx;

  struct segment *prev_seg;
  struct segment *next_seg;
  int32_t seg_id;
  double penalty;
  int n_skipped_penalty;

  int32_t n_total_hit;
  int32_t n_total_active;

  int32_t create_rtime;
  int64_t create_vtime;
  double req_rate; /* req rate when create the seg */
  double write_rate;
  double write_ratio;
  double cold_miss_ratio;

  int64_t become_train_seg_vtime;
  int64_t become_train_seg_rtime;

  seg_feature_t feature;

  int n_merge; /* number of merged times */
  bool is_training_seg;


  bool in_training_data;
  unsigned int training_data_row_idx;

  int32_t magic;
} segment_t;

typedef struct hitProb {
  int32_t n_hit[HIT_PROB_MAX_AGE];
  int32_t n_evict[HIT_PROB_MAX_AGE];

  double hit_density[HIT_PROB_MAX_AGE];
  int64_t n_overflow;
  int32_t age_shift;
} hitProb_t;

typedef struct {
  int64_t bucket_property;
  segment_t *first_seg;
  segment_t *last_seg;
  int32_t n_seg;
  int16_t bucket_idx;
  segment_t *next_seg_to_evict;

  hitProb_t *hit_prob;
} bucket_t;

typedef struct seg_sel {
  int64_t last_rank_time;
  segment_t **ranked_segs;
  int32_t ranked_seg_size;
  int32_t ranked_seg_pos;

  double *score_array;
  int score_array_size;

  //  double *obj_penalty;
  //  int obj_penalty_array_size;

  segment_t **segs_to_evict;
} seg_sel_t;

typedef struct {
  int segment_size; /* in temrs of number of objects */
  int n_merge;
  int n_retain_from_seg;

  int n_used_buckets;
  bucket_t buckets[MAX_N_BUCKET];
  bucket_t training_bucket; /* segments that are evicted and used for training */

  int curr_evict_bucket_idx;
  segment_t *next_evict_segment;

  int32_t n_segs;
  int32_t n_training_segs;
  int64_t n_allocated_segs;
  int64_t n_evictions;

  int64_t start_rtime;
  int64_t curr_rtime;
  int64_t curr_vtime;

  seg_sel_t seg_sel;

  learner_t learner;

  //  int64_t last_hit_prob_compute_rtime;
  int64_t last_hit_prob_compute_vtime;

  cache_state_t cache_state;

  int size_bucket_base;
  LSC_type_e type;
  bucket_type_e bucket_type;
  obj_score_e obj_score_type;
  int rank_intvl; /* in number of evictions */
} L2Cache_params_t;

cache_t *L2Cache_init(common_cache_params_t ccache_params, void *cache_specific_params);

void L2Cache_free(cache_t *cache);

cache_ck_res_e L2Cache_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e L2Cache_get(cache_t *cache, request_t *req);

void L2Cache_insert(cache_t *L2Cache, request_t *req);

void L2Cache_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void L2Cache_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

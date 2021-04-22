//
//  LLSC.h
//  libCacheSim
//

#pragma once

#include "../cache.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_N_BUCKET 1
#define N_TIME_WINDOW 80
#define MAGIC     1234567890

#define DEFAULT_RANK_INTVL 20
typedef enum {
  SEGCACHE,
  SEGCACHE_ITEM_ORACLE,
  SEGCACHE_SEG_ORACLE,
  SEGCACHE_BOTH_ORACLE,


  LOGCACHE_START_POS,

  LOGCACHE_BOTH_ORACLE,
  LOGCACHE_LOG_ORACLE,
  LOGCACHE_ITEM_ORACLE,
  LOGCACHE_LEARNED,
  LOGCACHE_RAMCLOUD,
  LOGCACHE_FIFO,
} LSC_type_e;

typedef struct {
  int segment_size;
  int n_merge;
  LSC_type_e type;
  int rank_intvl;
} LLSC_init_params_t;

typedef struct {

} seg_feature_t;

typedef struct segment {
//  cache_obj_t *first_obj;
//  cache_obj_t *last_obj;
  cache_obj_t *objs;
  int64_t total_byte;
  int32_t n_total_obj;
  int32_t create_time;

  struct segment *prev_seg;
  struct segment *next_seg;
  bool is_training_seg;
  int32_t magic;
  int32_t seg_id;
  double penalty;
} segment_t;

typedef struct {
  int64_t bucket_property;
  segment_t *first_seg;
  segment_t *last_seg;
  int32_t n_seg;
  int16_t bucket_idx;

} bucket_t;

typedef struct seg_sel{
  int64_t last_rank_time;
  segment_t **ranked_segs;
  int32_t ranked_seg_size;
  int32_t ranked_seg_pos;


  double *score_array;
  int score_array_size;

  double *obj_penalty;
  int obj_penalty_array_size;


} seg_sel_t;

typedef struct {
  int segment_size;   /* in temrs of number of objects */
  int n_merge;

  int n_used_buckets;
  bucket_t buckets[MAX_N_BUCKET];
  bucket_t training_bucket; /* segments that are evicted and used for training */

  int curr_evict_bucket_idx;
  segment_t *next_evict_segment;

  int64_t n_segs;
  int64_t n_training_segs;
  int64_t n_allocated_segs;
  int64_t n_evictions;

  int64_t curr_time;
  int64_t curr_vtime;

  int32_t full_cache_write_time;    /* real time */
  int32_t last_cache_full_time;     /* real time */
  int64_t n_byte_written;       /* cleared after each full cache write */
  int32_t time_window;              /* real time */

  seg_sel_t seg_sel;
  segment_t **segs_to_evict;

  LSC_type_e type;
  int rank_intvl; /* in number of evictions */
  bool start_feature_recording;
} LLSC_params_t;


cache_t *LLSC_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void LLSC_free(cache_t *cache);

cache_ck_res_e LLSC_check(cache_t *cache, request_t *req, bool update_cache);

cache_ck_res_e LLSC_get(cache_t *cache, request_t *req);

void LLSC_insert(cache_t *LLSC, request_t *req);

void LLSC_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void LLSC_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif


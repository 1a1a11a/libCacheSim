#pragma once


#ifndef SEGCACHE_H
#define SEGCACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cache.h"

struct obj_info {
  double score;
  int32_t size;
};

typedef struct segment {
  cache_obj_t *first_obj;
  cache_obj_t *last_obj;

  int32_t     n_obj;
  int32_t     n_live_obj;
  int64_t     total_byte;
  int64_t     live_byte;
  int64_t     creation_time;
  int64_t     ttl;


  struct segment *prev_seg;
  struct segment *next_seg;
} segment_t;

typedef struct segcache_params {
  int64_t segment_size;
  int64_t n_segment;
  int32_t n_merge;
  int32_t min_mature_time;

  struct segment *first_seg;
  struct segment *next_seg_to_merge;
  struct segment *last_seg;
  struct segment *free_seg;

//  struct segment *all_seg;
} segcache_params_t;


typedef struct segcache_init_params {
  int64_t segment_size;
  int32_t n_merge;
  int32_t min_mature_time;
} segcache_init_params_t;

cache_t *SEGCACHE_init(common_cache_params_t ccache_params,
                  void *cache_specific_params);

void SEGCACHE_free(cache_t *cache);

cache_ck_res_e SEGCACHE_check(cache_t *cache, request_t *req, bool update);

cache_ck_res_e SEGCACHE_get(cache_t *cache, request_t *req);

void SEGCACHE_remove(cache_t *cache, obj_id_t obj_id);

void SEGCACHE_insert(cache_t *cache, request_t *req);

void SEGCACHE_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

#ifdef __cplusplus
}
#endif

#endif  /* SEGCACHE_H */

#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

static inline bool is_seg_evictable_fifo(segment_t *seg, int min_evictable) {
  if (seg == NULL) return false;

  int n_evictable = 0;
  while (seg->next_seg != NULL && n_evictable < min_evictable) {
    n_evictable += 1;
    seg = seg->next_seg;
  }
  return n_evictable == min_evictable;
}

segment_t *allocate_new_seg(cache_t *cache, int bucket_idx);

void link_new_seg_before_seg(L2Cache_params_t *params, bucket_t *bucket, segment_t *old_seg,
                             segment_t *new_seg);

double find_cutoff(cache_t *cache, obj_score_type_e obj_score_type, segment_t **segs,
                   int n_segs, int n_retain);

double cal_seg_penalty(cache_t *cache, obj_score_type_e obj_score_type, segment_t *seg,
                       int n_retain, int64_t rtime, int64_t vtime);

void print_seg(cache_t *cache, segment_t *seg, int log_level);

int clean_one_seg(cache_t *cache, segment_t *seg);

int count_n_obj_reuse(cache_t *cache, segment_t *seg);

void print_seg(cache_t *cache, segment_t *seg, int log_level);

#pragma once

#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "GLCacheInternal.h"

// check whether there are min_evictable segments to evict 
// consecutive indicates the merge eviction uses consecutive segments in the chain
// if not, it usees segments in ranked order and may not be consecutive 
static inline bool is_seg_evictable(segment_t *seg, int min_evictable, bool consecutive) {
  if (seg == NULL) return false;

  int n_evictable = 0;
  while (seg != NULL && seg->next_seg != NULL) {
    n_evictable += 1;
    if (n_evictable >= min_evictable) return true;

    if (consecutive) {
      seg = seg->next_seg;
    } else {
      seg = seg->next_ranked_seg;
    }
  }
  return n_evictable >= min_evictable;
}

segment_t *allocate_new_seg(cache_t *cache, int bucket_id);

void link_new_seg_before_seg(GLCache_params_t *params, bucket_t *bucket, segment_t *old_seg,
                             segment_t *new_seg);

double find_cutoff(cache_t *cache, obj_score_type_e obj_score_type, segment_t **segs,
                   int n_segs, int n_retain);

double cal_seg_utility(cache_t *cache, segment_t *seg, bool oracle_obj_sel); 

int clean_one_seg(cache_t *cache, segment_t *seg);

int count_n_obj_reuse(cache_t *cache, segment_t *seg);

void clear_dynamic_features(cache_t *cache); 

void print_seg(cache_t *cache, segment_t *seg, int log_level);


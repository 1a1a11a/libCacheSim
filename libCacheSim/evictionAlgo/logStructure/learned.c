

#include "learned.h"

#include <LightGBM/c_api.h>





void clean_training_segs(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  segment_t *seg = params->training_bucket.first_seg;
  int n_clean = 0;

  while (seg != NULL) {
    segment_t *next_seg = seg->next_seg;
    clean_one_seg(cache, seg);
    seg = next_seg;
    n_clean += 1;
  }
  DEBUG_ASSERT(n_clean == params->training_bucket.n_seg);
  params->n_training_segs = 0;
  params->training_bucket.n_seg = 0;
  params->training_bucket.first_seg = NULL;
  params->training_bucket.last_seg = NULL;
}

static double cal_seg_evict_penalty_learned(cache_t *cache, segment_t *seg) {
  ;
}

void training() {
  ;
}

void inference() {
  ;
}


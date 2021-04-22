

#include "oracle.h"


void cal_seg_evict_penalty_oracle(cache_t *cache, segment_t *seg, int curr_vtime) {
  LLSC_params_t *params = cache->cache_params;
  if (seg->next_seg == NULL) {
    seg->penalty = INT32_MAX;
    return;
  }

  if (params->seg_sel.obj_penalty_array_size < seg->n_total_obj) {
    if (params->seg_sel.obj_penalty != NULL) {
      my_free(sizeof(double) * array_size, params->seg_sel.obj_penalty);
    }
    params->seg_sel.obj_penalty = my_malloc_n(double, seg->n_total_obj);
    params->seg_sel.obj_penalty_array_size = seg->n_total_obj;
  }

  for (int i = 0; i < seg->n_total_obj; i++) {
    if (seg->objs[i].next_access_ts == -1) {
      params->seg_sel.obj_penalty[i] = 0;
    } else {
      params->seg_sel.obj_penalty[i] = 1000000.0 / (double) ((seg->objs[i].next_access_ts - curr_vtime) * seg->objs[i].obj_size);
    }
    DEBUG_ASSERT(params->seg_sel.obj_penalty[i] >= 0);
  }
  qsort(params->seg_sel.obj_penalty, seg->n_total_obj, sizeof(double), cmp_double);

  double penalty = 0;
  int n_retained_obj = seg->n_total_obj / params->n_merge;

  for (int i = 0; i < seg->n_total_obj - n_retained_obj; i++)
    penalty += params->seg_sel.obj_penalty[n_retained_obj + i];

  seg->penalty = penalty;
}

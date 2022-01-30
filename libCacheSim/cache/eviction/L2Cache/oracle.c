

#include "oracle.h"
#include "../../../dataStructure/hashtable/hashtable.h"
#include "L2CacheInternal.h"
#include "learned.h"

//void cal_seg_evict_penalty_oracle(cache_t *cache, segment_t *seg, int curr_vtime) {
//  L2Cache_params_t *params = cache->eviction_params;
//  if (seg->next_seg == NULL) {
//    seg->penalty = INT32_MAX;
//    return;
//  }
//
//  if (params->seg_sel.obj_penalty_array_size < seg->n_obj) {
//    if (params->seg_sel.obj_penalty != NULL) {
//      my_free(sizeof(double) * array_size, params->seg_sel.obj_penalty);
//    }
//    params->seg_sel.obj_penalty = my_malloc_n(double, seg->n_obj);
//    params->seg_sel.obj_penalty_array_size = seg->n_obj;
//  }
//
//  for (int i = 0; i < seg->n_obj; i++) {
//    if (seg->objs[i].next_access_vtime == -1) {
//      params->seg_sel.obj_penalty[i] = 0;
//    } else {
//      params->seg_sel.obj_penalty[i] = 1000000.0 / (double) ((seg->objs[i].next_access_vtime - curr_vtime) * seg->objs[i].obj_size);
//    }
//    DEBUG_ASSERT(params->seg_sel.obj_penalty[i] >= 0);
//  }
//  qsort(params->seg_sel.obj_penalty, seg->n_obj, sizeof(double), cmp_double);
//
//  double penalty = 0;
//  int n_retained_obj = seg->n_obj / params->n_merge;
//
//  for (int i = 0; i < seg->n_obj - n_retained_obj; i++)
//    penalty += params->seg_sel.obj_penalty[n_retained_obj + i];
//
//  seg->penalty = penalty;
//}

cache_ck_res_e L2Cache_check_oracle(cache_t *cache, request_t *req, bool update_cache) {
  L2Cache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    return cache_ck_hit;
  }

  if (cache_obj->L2Cache.in_cache) {
    seg_hit(params, cache_obj);
    object_hit(params, cache_obj, req);

    return cache_ck_hit;
  } else {

    return cache_ck_miss;
  }
}

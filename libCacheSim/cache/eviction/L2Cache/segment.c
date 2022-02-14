

#include "segment.h"
#include "../../dataStructure/hashtable/hashtable.h"
#include "const.h"
#include "obj.h"
#include "utils.h"

int clean_one_seg(cache_t *cache, segment_t *seg) {
  L2Cache_params_t *params = cache->eviction_params;
  int n_cleaned = 0;
  DEBUG_ASSERT(seg->n_obj == params->segment_size);
  for (int i = 0; i < seg->n_obj; i++) {
    cache_obj_t *cache_obj = &seg->objs[i];
    if (hashtable_try_delete(cache->hashtable, cache_obj)) {
      n_cleaned += 1;
    }
  }
  my_free(sizeof(cache_obj_t) * params->n_obj, seg->objs);
  my_free(sizeof(segment_t), seg);

  return n_cleaned;
}

segment_t *allocate_new_seg(cache_t *cache, int bucket_id) {
  L2Cache_params_t *params = cache->eviction_params;

  /* allocate a new segment */
  segment_t *new_seg = my_malloc(segment_t);
  memset(new_seg, 0, sizeof(segment_t));
  new_seg->objs = my_malloc_n(cache_obj_t, params->segment_size);
  memset(new_seg->objs, 0, sizeof(cache_obj_t) * params->segment_size);

  new_seg->next_seg = NULL;
  new_seg->n_obj = new_seg->n_byte = 0;
  new_seg->create_rtime = params->curr_rtime;
  new_seg->create_vtime = params->curr_vtime;

  new_seg->selected_for_training = false;
  new_seg->pred_utility = INT64_MAX;// to avoid it being picked for eviction
  new_seg->train_utility = 0;
  new_seg->magic = MAGIC;
  new_seg->seg_id = params->n_allocated_segs++;
  new_seg->bucket_id = bucket_id;

  return new_seg;
}

void link_new_seg_before_seg(L2Cache_params_t *params, bucket_t *bucket, segment_t *old_seg,
                             segment_t *new_seg) {
  DEBUG_ASSERT(new_seg->bucket_id == bucket->bucket_id);
  DEBUG_ASSERT(old_seg->next_seg->bucket_id == bucket->bucket_id);

  if (old_seg->prev_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == old_seg);
    bucket->first_seg = new_seg;
  } else {
    old_seg->prev_seg->next_seg = new_seg;
  }
  new_seg->prev_seg = old_seg->prev_seg;
  new_seg->next_seg = old_seg;
  old_seg->prev_seg = new_seg;

  params->n_segs += 1;
  bucket->n_segs += 1;
}

int count_n_obj_reuse(cache_t *cache, segment_t *seg) {
  int n = 0;
  for (int i = 0; i < seg->n_obj; i++) {
    if (seg->objs[i].L2Cache.next_access_vtime > 0) {
      n += 1;
    }
  }
  return n;
}

/* find the cutoff object score to retain objects, this is used in merging multiple segments into one */
double find_cutoff(cache_t *cache, obj_score_type_e obj_score_type, segment_t **segs,
                   int n_segs, int n_retain) {
  L2Cache_params_t *params = cache->eviction_params;

  segment_t *seg;
  int pos = 0;

  for (int i = 0; i < n_segs; i++) {
    seg = segs[i];
    for (int j = 0; j < seg->n_obj; j++) {
      params->obj_sel.score_array[pos++] = cal_object_score(
          params, obj_score_type, &seg->objs[j], params->curr_rtime, params->curr_vtime);
    }
  }
  DEBUG_ASSERT(pos == params->segment_size * n_segs);
  qsort(params->obj_sel.score_array, pos, sizeof(double), cmp_double);

  return params->obj_sel.score_array[pos - n_retain];
}

/** calculate segment utility, and a segment with a lower utility should be evicted first **/
double cal_seg_utility(cache_t *cache, obj_score_type_e obj_score_type, segment_t *seg,
                       int64_t rtime, int64_t vtime) {
  L2Cache_params_t *params = cache->eviction_params;
  seg_sel_t *seg_sel = &params->seg_sel;
  obj_sel_t *obj_sel = &params->obj_sel;
  cache_obj_t *cache_obj; 

  DEBUG_ASSERT(seg->n_obj <= obj_sel->score_array_size);

  for (int j = 0; j < seg->n_obj; j++) {
    // obj_sel->score_array[j] = 
        // cal_object_score(params, obj_score_type, &seg->objs[j], rtime, vtime);
    cache_obj = &seg->objs[j]; 
    double age = rtime - cache_obj->L2Cache.last_access_rtime; 
    obj_sel->score_array[j] = 1.0e6 / age / cache_obj->obj_size; 
  }

  double utility = 0;
  int n_retained_obj = 0; 
#ifdef TRAINING_CONSIDER_RETAIN
  n_retained_obj = params->n_retain_per_seg;
  qsort(obj_sel->score_array, seg->n_obj, sizeof(double), cmp_double);
  DEBUG_ASSERT(obj_sel->score_array[0] <= obj_sel->score_array[seg->n_obj - 1]);
#else 
  for (int j = 0; j < seg->n_obj - n_retained_obj; j++) {
    utility += obj_sel->score_array[j];
  }
#endif

  return utility;
}

void print_seg(cache_t *cache, segment_t *seg, int log_level) {
  L2Cache_params_t *params = cache->eviction_params;

  printf("seg %6d, age %6d, mean obj size %8.0lf bytes, "
         "req/write rate %6.0lf/%4.2lf, "
         "miss ratio %.4lf, "
         "mean freq %4.2lf, total hit %6d, total active %4d, "
         "%2d merges, utility %.4lf/%.4lf, oracle_score %lf, %d obj have reuse, "
         "n_hit/active window %d %d %d %d, \n",
         seg->seg_id, (int) params->curr_rtime - seg->create_rtime,
         (double) seg->n_byte / seg->n_obj, seg->req_rate, seg->write_rate, seg->miss_ratio,
         (double) seg->n_hit / seg->n_obj, seg->n_hit, seg->n_active, seg->n_merge,
         seg->train_utility, seg->pred_utility,
         cal_seg_utility(cache, OBJ_SCORE_ORACLE, seg, 
                         params->curr_rtime, params->curr_vtime),
         count_n_obj_reuse(cache, seg),

         seg->feature.n_hit_per_min[0], seg->feature.n_hit_per_min[1],
         seg->feature.n_hit_per_min[2], seg->feature.n_hit_per_min[3]);
}

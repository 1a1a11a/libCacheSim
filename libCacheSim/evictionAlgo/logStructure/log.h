#pragma once

//#include

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"

#include "../../dataStructure/hashtable/hashtable.h"

//static inline void add_to_seg(cache_t *cache, segment_t *segment, cache_obj_t *cache_obj, bool is_training_seg) {
//  segment->n_total_obj += 1;
//  segment->total_byte += cache_obj->obj_size + cache->per_obj_overhead;
//  cache_obj->ref_cnt += 1;
//
//  if (segment->last_obj == NULL) {
//    segment->first_obj = cache_obj;
//  } else {
//    segment->last_obj->list_next = cache_obj;
//  }
//  segment->last_obj = cache_obj;
//
//  /*
//   * an object can exist in two seg, data seg, or training seg
//   * we use list_prev_void to point to its original seg,
//   * extra_metadata_ptr2 to point to the training seg
//   */
//  if (is_training_seg) {
//    cache_obj->list_prev_void = segment;
//  } else {
//    cache_obj->segment = segment;
//  }
//}

static inline void debug_check_bucket(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  segment_t *curr_seg;
  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      n_seg += 1;
      DEBUG_ASSERT(curr_seg != NULL);
      curr_seg = curr_seg->next_seg;
    }
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_segs == n_seg);

  n_seg = 0;
  curr_seg = params->training_bucket.first_seg;
  for (int si = 0; si < params->training_bucket.n_seg; si++) {
    n_seg += 1;
    DEBUG_ASSERT(curr_seg != NULL);
    curr_seg = curr_seg->next_seg;
  }
  DEBUG_ASSERT(curr_seg == NULL);
  DEBUG_ASSERT(params->n_training_segs == n_seg);
}

static inline int cmp_double(const void *p1, const void *p2) {
  if (*(double *) p1 < *(double *) p2)
    return -1;
  else
    return 1;
}

static inline int LLSC_find_bucket_idx(request_t *req) {
  return 0;
}

static inline bool is_seg_evictable_fifo(segment_t *seg, int min_evictable) {
  if (seg == NULL)
    return false;

  int n_evictable = 0;
  while (seg->next_seg != NULL && n_evictable < min_evictable) {
    n_evictable += 1;
    seg = seg->next_seg;
  }
  return n_evictable == min_evictable;
}

__attribute__((unused)) static inline bool
obj_in_hashtable(cache_t *cache, cache_obj_t *cache_obj) {
  cache_obj_t *obj = hashtable_find_obj(cache->hashtable, cache_obj);
  bool find = false;
  while (obj != NULL) {
    if (obj == cache_obj) {
      find = true;
      break;
    }
    obj = obj->hash_next;
  }
  return find;
}

static inline void clean_one_seg(cache_t *cache, segment_t *seg) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj;

  DEBUG_ASSERT(seg->n_total_obj == params->segment_size);
  //  for (int i = 0; i < seg->n_total_obj; i++) {
  //    cache_obj = &seg->objs[i];
  //    hashtable_delete(cache->hashtable, cache_obj);
  //  }
  my_free(sizeof(cache_obj_t) * params->n_total_obj, seg->objs);
  my_free(sizeof(segment_t), seg);
}

static inline void remove_seg_from_bucket(bucket_t *bucket, segment_t *segment) {
  if (bucket->first_seg == segment) {
    bucket->first_seg = segment->next_seg;
  }
  if (bucket->last_seg == segment) {
    bucket->last_seg = segment->prev_seg;
  }

  if (segment->prev_seg != NULL) {
    segment->prev_seg->next_seg = segment->next_seg;
  }

  if (segment->next_seg != NULL) {
    segment->next_seg->prev_seg = segment->prev_seg;
  }
}

static inline void append_seg_to_bucket(bucket_t *bucket, segment_t *segment) {
  /* because the last segment may not be full, so we link before it */
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    DEBUG_ASSERT(bucket->n_seg == 0);
    bucket->first_seg = segment;
  } else {
    bucket->last_seg->next_seg = segment;
  }

  segment->prev_seg = bucket->last_seg;
  segment->next_seg = NULL;
  bucket->last_seg = segment;
}

static inline void append_seg_to_bucket_before_last(bucket_t *bucket, segment_t *segment) {
  /* because the last segment may not be full, so we link before it */
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    DEBUG_ASSERT(bucket->n_seg == 0);
    bucket->first_seg = segment;
  } else {
    segment->prev_seg = bucket->last_seg->prev_seg;
    if (bucket->last_seg->prev_seg == NULL) {
      DEBUG_ASSERT(bucket->n_seg == 1);
      bucket->first_seg = segment;
    } else {
      bucket->last_seg->prev_seg->next_seg = segment;
    }
    bucket->last_seg->prev_seg = segment;
    segment->next_seg = bucket->last_seg;
  }
}

static inline segment_t *allocate_new_seg(cache_t *cache, int bucket_idx) {
  LLSC_params_t *params = cache->cache_params;

  /* allocate a new segment */
  segment_t *new_seg = my_malloc(segment_t);
  memset(new_seg, 0, sizeof(segment_t));
  new_seg->objs = my_malloc_n(cache_obj_t, params->segment_size);
  DEBUG_ASSERT(new_seg->objs != NULL);
  memset(new_seg->objs, 0, sizeof(cache_obj_t) * params->segment_size);

  new_seg->next_seg = NULL;
  new_seg->n_total_obj = new_seg->total_byte = 0;
  new_seg->create_rtime = params->curr_time;
  new_seg->create_vtime = params->curr_vtime;
  new_seg->is_training_seg = false;
  new_seg->magic = MAGIC;
  new_seg->seg_id = params->n_allocated_segs++;
  new_seg->bucket_idx = bucket_idx;

  return new_seg;
}

static inline void link_new_seg_before_seg(bucket_t *bucket, segment_t *old_seg, segment_t *new_seg) {
  if (old_seg->prev_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == old_seg);
    bucket->first_seg = new_seg;
  } else {
    old_seg->prev_seg->next_seg = new_seg;
  }
  new_seg->prev_seg = old_seg->prev_seg;
  new_seg->next_seg = old_seg;
  old_seg->prev_seg = new_seg;
}

static inline double cal_object_score(obj_score_e score_type, cache_obj_t *cache_obj, int curr_rtime, int64_t curr_vtime) {

  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->LSC.LLSC_freq;
  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1000 / cache_obj->obj_size;
  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->LSC.next_access_ts == -1) {
      return 0;
    }
    return (double) 1000000.0 / (double) (cache_obj->LSC.next_access_ts - curr_vtime) / cache_obj->obj_size;
  } else if (score_type == OBJ_SCORE_FREQ_BYTE_AGE) {
    DEBUG_ASSERT(curr_rtime >= 0);
    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1000 / cache_obj->obj_size / (curr_rtime - cache_obj->LSC.last_access_rtime);
  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

static inline double find_cutoff(cache_t *cache, obj_score_e obj_score_type, segment_t **segs, int n_segs, int n_retain) {
  LLSC_params_t *params = cache->cache_params;

  segment_t *seg;
  int pos = 0;

  for (int i = 0; i < n_segs; i++) {
    seg = segs[i];
    for (int j = 0; j < seg->n_total_obj; j++) {
      params->seg_sel.score_array[pos++] = cal_object_score(obj_score_type, &seg->objs[j], params->curr_time, params->curr_vtime);
    }
  }
  DEBUG_ASSERT(pos == params->segment_size * n_segs);
  qsort(params->seg_sel.score_array, pos, sizeof(double), cmp_double);

  return params->seg_sel.score_array[pos - n_retain];
}

static inline double cal_seg_penalty(cache_t *cache, obj_score_e obj_score_type, segment_t *seg,
                                     int n_retain, int64_t rtime, int64_t vtime) {
  LLSC_params_t *params = cache->cache_params;
  seg_sel_t *seg_sel = &params->seg_sel;

  int pos = 0;
  for (int j = 0; j < seg->n_total_obj; j++) {
    seg_sel->score_array[pos++] = cal_object_score(
        obj_score_type, &seg->objs[j], rtime, vtime);
  }

  DEBUG_ASSERT(pos <= seg_sel->score_array_size);
  qsort(seg_sel->score_array, pos, sizeof(double), cmp_double);
  DEBUG_ASSERT(seg_sel->score_array[0] <= seg_sel->score_array[pos - 1]);

  //  if (seg_sel->score_array[0] == seg_sel->score_array[pos - 1]) {
  //    printf("seg may have all objects with no reuse\n");
  //  }

  double penalty = 0;
  for (int j = 0; j < seg->n_total_obj - n_retain; j++) {
    penalty += seg_sel->score_array[j];
  }

  //  penalty = penalty * 1000000 / seg->total_byte;
  return penalty;
}

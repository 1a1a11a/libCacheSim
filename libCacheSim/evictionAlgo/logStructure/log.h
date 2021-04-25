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

static inline int64_t object_age(LLSC_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->LSC.last_access_rtime;
}

static inline void object_hit(LLSC_params_t *params, cache_obj_t *obj, request_t *req) {
  obj->LSC.next_access_ts = req->next_access_ts;
  obj->LSC.LLSC_freq += 1;

  segment_t *seg = obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  int64_t obj_age = object_age(params, obj);
  obj_age = obj_age >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age;
  bkt->hit_prob.n_hit[obj_age] += 1;
  obj->LSC.last_access_rtime = params->curr_rtime;
}

static inline void object_evict(LLSC_params_t *params, cache_obj_t *obj) {
  segment_t *seg = obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  int64_t obj_age = object_age(params, obj);
  obj_age = obj_age >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age;
  bkt->hit_prob.n_evict[obj_age] += 1;
}

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
  new_seg->create_rtime = params->curr_rtime;
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

static inline double cal_object_score(LLSC_params_t *params,
                                      obj_score_e score_type,
                                      cache_obj_t *cache_obj,
                                      int curr_rtime,
                                      int64_t curr_vtime) {
  segment_t *seg = cache_obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->LSC.LLSC_freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1000 / cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_HIT_DENSITY) {
    int64_t obj_age = object_age(params, cache_obj);
    obj_age = obj_age >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age;
    return bkt->hit_prob.hit_density[obj_age] / cache_obj->obj_size;

//  } else if (score_type == OBJ_SCORE_HIT_DENSITY_FREQ) {
//    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1000 * bkt->hit_prob.hit_density[object_age(params, cache_obj)] / cache_obj->obj_size;
//
  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->LSC.next_access_ts == -1) {
      return 0;
    }
    return (double) 1000000.0 / (double) cache_obj->obj_size / (double) (cache_obj->LSC.next_access_ts - curr_vtime);

//  } else if (score_type == OBJ_SCORE_FREQ_BYTE_AGE) {
//    DEBUG_ASSERT(curr_rtime >= 0);
//    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1000 / cache_obj->obj_size / (curr_rtime - cache_obj->LSC.last_access_rtime);
//
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
      params->seg_sel.score_array[pos++] = cal_object_score(
          params, obj_score_type, &seg->objs[j], params->curr_rtime, params->curr_vtime);
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
        params, obj_score_type, &seg->objs[j], rtime, vtime);
  }

  DEBUG_ASSERT(pos <= seg_sel->score_array_size);
  qsort(seg_sel->score_array, pos, sizeof(double), cmp_double);
  DEBUG_ASSERT(seg_sel->score_array[0] <= seg_sel->score_array[pos - 1]);

  static int n_err = 0;
  if (seg_sel->score_array[0] == seg_sel->score_array[pos - 1]) {
    if (n_err++ % 1000 == 0)
      WARNING("cache size %lu: seg may have all objects with no reuse\n",
             (unsigned long) cache->cache_size);
    abort();
  }

  double penalty = 0;
  for (int j = 0; j < seg->n_total_obj - n_retain; j++) {
    penalty += seg_sel->score_array[j];
  }

  return penalty;
}

//static inline void bucket_hit(bucket_t *bkt, ) {
//  bkt
//}

static inline void update_hit_prob_cdf(bucket_t *bkt) {
  hitProb_t *hb = &bkt->hit_prob;
  int64_t n_hit_sum = hb->n_hit[HIT_PROB_MAX_AGE - 1];
  int64_t n_event_sum = n_hit_sum + hb->n_evict[HIT_PROB_MAX_AGE - 1];
  int64_t lifetime_uncond = n_event_sum;

  for (int i = HIT_PROB_MAX_AGE - 2; i >= 0; i--) {
    n_hit_sum += hb->n_hit[i];
    n_event_sum += hb->n_hit[i] + hb->n_evict[i];
    lifetime_uncond += n_event_sum;

    hb->n_hit[i] *= LHD_EWMA;
    hb->n_evict[i] *= LHD_EWMA;

    if (n_event_sum > 0) {
      hb->hit_density[i] = (double) n_hit_sum * 1e10 / lifetime_uncond;
    } else {
      hb->hit_density[i] = 1e-8;
    }
  }
}

static inline void print_seg(cache_t *cache, segment_t *seg) {
  LLSC_params_t *params = cache->cache_params;
  char s[1024];

  printf("seg mean obj size %.2lf bytes, "
         "req/write rate %.4lf/%.4lf, "
         "age %d, mean freq %.2lf, total hit %d, total active %d, "
         "%d merges, ",
         (double) seg->total_byte / seg->n_total_obj,
         seg->req_rate, seg->write_rate,
         (int) params->curr_rtime - seg->create_rtime,
         (double) seg->n_total_hit / seg->n_total_active,
         seg->n_total_hit, seg->n_total_active,
         seg->n_merge);

  printf("n hit %d %d %d %d %d %d %d %d %d %d %d %d, "
         "n_active_per_window "
         "%d %d %d %d %d %d %d %d %d %d %d %d, "
         "active_accu %d %d %d %d %d %d %d %d %d %d %d %d\n",
         seg->feature.n_hit[0], seg->feature.n_hit[1], seg->feature.n_hit[2], seg->feature.n_hit[3],
         seg->feature.n_hit[4], seg->feature.n_hit[5], seg->feature.n_hit[6], seg->feature.n_hit[7],
         seg->feature.n_hit[8], seg->feature.n_hit[9], seg->feature.n_hit[10], seg->feature.n_hit[11],

         seg->feature.n_active_item_per_window[0], seg->feature.n_active_item_per_window[1], seg->feature.n_active_item_per_window[2], seg->feature.n_active_item_per_window[3],
         seg->feature.n_active_item_per_window[4], seg->feature.n_active_item_per_window[5], seg->feature.n_active_item_per_window[6], seg->feature.n_active_item_per_window[7],
         seg->feature.n_active_item_per_window[8], seg->feature.n_active_item_per_window[9], seg->feature.n_active_item_per_window[10], seg->feature.n_active_item_per_window[11],

         seg->feature.n_active_item_accu[0], seg->feature.n_active_item_accu[1], seg->feature.n_active_item_accu[2], seg->feature.n_active_item_accu[3],
         seg->feature.n_active_item_accu[4], seg->feature.n_active_item_accu[5], seg->feature.n_active_item_accu[6], seg->feature.n_active_item_accu[7],
         seg->feature.n_active_item_accu[8], seg->feature.n_active_item_accu[9], seg->feature.n_active_item_accu[10], seg->feature.n_active_item_accu[11]);
}


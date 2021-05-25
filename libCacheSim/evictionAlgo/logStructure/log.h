#pragma once

//#include

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"

#include "../../dataStructure/hashtable/hashtable.h"

static inline void print_seg(cache_t *cache, segment_t *seg, int log_level);

static inline void _debug_check_bucket_segs(bucket_t *bkt) {
  segment_t *curr_seg = bkt->first_seg;
  int n_seg = 0;
  segment_t *prev_seg = NULL;

  while (curr_seg) {
    n_seg += 1;
    assert(curr_seg->prev_seg == prev_seg);
    assert(curr_seg->bucket_idx == bkt->bucket_idx);
    prev_seg = curr_seg;
    curr_seg = curr_seg->next_seg;
  }

  assert(prev_seg == bkt->last_seg);
  assert(n_seg == bkt->n_seg);
}

static inline int _debug_count_n_obj(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  int64_t n_obj = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    segment_t *curr_seg = params->buckets[i].first_seg;
    int n_seg_in_bucket = 0;
    while (curr_seg) {
      if (curr_seg->n_total_obj != params->segment_size) {
        printf("find segment not full, bucket %d, seg %d: %d objects training %d\n", i,
               n_seg_in_bucket, curr_seg->n_total_obj, curr_seg->is_training_seg);
      }
      n_obj += curr_seg->n_total_obj;
      curr_seg = curr_seg->next_seg;
      n_seg_in_bucket++;
    }
  }

  return n_obj;
}

static inline int64_t object_age(LLSC_params_t *params, cache_obj_t *obj) {
  return params->curr_rtime - obj->LSC.last_access_rtime;
}

static inline int64_t object_age_shifted(LLSC_params_t *params, cache_obj_t *obj) {
  bucket_t *bkt = &params->buckets[((segment_t *) (obj->LSC.segment))->bucket_idx];
  int64_t obj_age =
      (params->curr_rtime - obj->LSC.last_access_rtime) >> bkt->hit_prob->age_shift;
  if (obj_age >= HIT_PROB_MAX_AGE) {
    bkt->hit_prob->n_overflow += 1;
    obj_age = HIT_PROB_MAX_AGE - 1;
  }
  return obj_age;
}

static inline void object_hit(LLSC_params_t *params, cache_obj_t *obj, request_t *req) {
  obj->LSC.next_access_ts = req->next_access_ts;
  obj->LSC.LLSC_freq += 1;

  segment_t *seg = obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  int64_t obj_age = object_age_shifted(params, obj);
  bkt->hit_prob->n_hit[obj_age] += 1;
  obj->LSC.last_access_rtime = params->curr_rtime;
}

static inline void object_evict(cache_t *cache, cache_obj_t *obj) {
  LLSC_params_t *params = cache->cache_params;
  segment_t *seg = obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

#ifdef TRACK_EVICTION_AGE
  record_eviction_age(cache, (int) (params->curr_rtime - obj->last_access_rtime));
#endif

  int64_t obj_age = object_age_shifted(params, obj);
  bkt->hit_prob->n_evict[obj_age] += 1;
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
  if (*(double *) p1 < *(double *) p2) return -1;
  else
    return 1;
}

static inline int find_bucket_idx(LLSC_params_t *params, request_t *req) {
  const double log_base = log(2);

  if (params->bucket_type == NO_BUCKET) {
    return 0;
  } else if (params->bucket_type == SIZE_BUCKET) {
    if (params->size_bucket_base == 1)
        return sizeof(unsigned int)*8 - 1 - __builtin_clz(req->obj_size);
    else
      return MAX(0, (int) (log((double) req->obj_size / params->size_bucket_base) / log_base));
//    return MAX(0, (int) (log(req->obj_size / 10.0) / log_base));
//    return MAX(0, (int) (log(req->obj_size / 120.0) / log_base));
  } else if (params->bucket_type == CUSTOMER_BUCKET) {
    return req->customer_id % 8;
  } else if (params->bucket_type == BUCKET_ID_BUCKET) {
    return req->bucket_id % 8;
  } else if (params->bucket_type == CONTENT_TYPE_BUCKET) {
    return req->content_type;
  } else {
    printf("unknown bucket type %d\n", params->bucket_type);
    abort();
  }
}

static inline bool is_seg_evictable_fifo(segment_t *seg, int min_evictable) {
  if (seg == NULL) return false;

  int n_evictable = 0;
  while (seg->next_seg != NULL && n_evictable < min_evictable) {
    n_evictable += 1;
    seg = seg->next_seg;
  }
  return n_evictable == min_evictable;
}

__attribute__((unused)) static inline bool obj_in_hashtable(cache_t *cache,
                                                            cache_obj_t *cache_obj) {
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

static inline int clean_one_seg(cache_t *cache, segment_t *seg) {
  LLSC_params_t *params = cache->cache_params;
  int n_cleaned = 0;
  DEBUG_ASSERT(seg->n_total_obj == params->segment_size);
  for (int i = 0; i < seg->n_total_obj; i++) {
    cache_obj_t *cache_obj = &seg->objs[i];
    if (hashtable_try_delete(cache->hashtable, cache_obj)) {
      n_cleaned += 1;
    }
  }
  my_free(sizeof(cache_obj_t) * params->n_total_obj, seg->objs);
  my_free(sizeof(segment_t), seg);

  return n_cleaned;
}

static inline void remove_seg_from_bucket(LLSC_params_t *params, bucket_t *bucket,
                                          segment_t *segment) {
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

  params->n_segs -= 1;
  bucket->n_seg -= 1;

  if (bucket->n_seg == 0) {
    params->n_used_buckets -= 1;
  }
}

static inline void append_seg_to_bucket(LLSC_params_t *params, bucket_t *bucket,
                                        segment_t *segment) {
  /* because the last segment may not be full, so we link before it */
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    DEBUG_ASSERT(bucket->n_seg == 0);
    bucket->first_seg = segment;
    bucket->next_seg_to_evict = segment;
    if (&params->training_bucket != bucket)
      params->n_used_buckets += 1;
  } else {
    bucket->last_seg->next_seg = segment;
  }

  segment->prev_seg = bucket->last_seg;
  segment->next_seg = NULL;
  bucket->last_seg = segment;

  bucket->n_seg += 1;
  if (&params->training_bucket == bucket) {
    params->n_training_segs += 1;
  } else {
    params->n_segs += 1;
  }
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
  memset(new_seg->objs, 0, sizeof(cache_obj_t) * params->segment_size);

  new_seg->next_seg = NULL;
  new_seg->n_total_obj = new_seg->total_bytes = 0;
  new_seg->create_rtime = params->curr_rtime;
  new_seg->create_vtime = params->curr_vtime;
  new_seg->is_training_seg = false;
  new_seg->in_training_data = false;
  new_seg->penalty = 0;
  new_seg->magic = MAGIC;
  new_seg->seg_id = params->n_allocated_segs++;
  new_seg->bucket_idx = bucket_idx;

  return new_seg;
}

static inline void link_new_seg_before_seg(LLSC_params_t *params, bucket_t *bucket,
                                           segment_t *old_seg, segment_t *new_seg) {
  DEBUG_ASSERT(new_seg->bucket_idx == bucket->bucket_idx);
  DEBUG_ASSERT(old_seg->next_seg->bucket_idx == bucket->bucket_idx);

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
  bucket->n_seg += 1;
}

static inline double cal_object_score(LLSC_params_t *params, obj_score_e score_type,
                                      cache_obj_t *cache_obj, int curr_rtime,
                                      int64_t curr_vtime) {
  segment_t *seg = cache_obj->LSC.segment;
  bucket_t *bkt = &params->buckets[seg->bucket_idx];

  if (score_type == OBJ_SCORE_FREQ) {
    return (double) cache_obj->LSC.LLSC_freq;

  } else if (score_type == OBJ_SCORE_FREQ_BYTE) {
    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1.0e6 / cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_FREQ_AGE) {
    return (double) (cache_obj->LSC.LLSC_freq + 0.01) * 1.0e6 / (curr_rtime - cache_obj->LSC.last_access_rtime);

  } else if (score_type == OBJ_SCORE_HIT_DENSITY) {
    int64_t obj_age = object_age(params, cache_obj);
    obj_age = obj_age >= HIT_PROB_MAX_AGE ? HIT_PROB_MAX_AGE - 1 : obj_age;
    return 1.0e6 * bkt->hit_prob->hit_density[obj_age] / cache_obj->obj_size;

  } else if (score_type == OBJ_SCORE_ORACLE) {
    if (cache_obj->LSC.next_access_ts == -1) {
      return 0;
    }

    DEBUG_ASSERT(cache_obj->LSC.next_access_ts > curr_vtime);
    return 1.0e8 / (double) cache_obj->obj_size
           / (double) (cache_obj->LSC.next_access_ts - curr_vtime);

  } else {
    printf("unknown cache type %d\n", score_type);
    abort();
  }
}

static inline int count_n_obj_reuse(cache_t *cache, segment_t *seg) {
  int n = 0;
  for (int i = 0; i < seg->n_total_obj; i++) {
    if (seg->objs[i].LSC.next_access_ts > 0) {
      n += 1;
    }
  }
  return n;
}

static inline double find_cutoff(cache_t *cache, obj_score_e obj_score_type, segment_t **segs,
                                 int n_segs, int n_retain) {
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
    seg_sel->score_array[pos++] =
        cal_object_score(params, obj_score_type, &seg->objs[j], rtime, vtime);
  }

  DEBUG_ASSERT(pos <= seg_sel->score_array_size);
  qsort(seg_sel->score_array, pos, sizeof(double), cmp_double);
  DEBUG_ASSERT(seg_sel->score_array[0] <= seg_sel->score_array[pos - 1]);

  static int n_err = 0;
  if (seg_sel->score_array[0] == seg_sel->score_array[pos - 1]) {
    if (n_err++ % 100000 == 20) {
      DEBUG("cache size %lu: seg may have all objects with no reuse %d (ignore this if "
              "it is end of trace running oracle)\n",
              (unsigned long) cache->cache_size, n_err);
      print_seg(cache, seg, DEBUG_LEVEL);
    }
    n_err += 1;
  }

  double penalty = 0;
  for (int j = 0; j < seg->n_total_obj - n_retain; j++) {
    penalty += seg_sel->score_array[j];
  }

  /* we add this term here because the segment here is not fix-sized */
//  penalty = penalty * 1e8 / seg->total_bytes;
  DEBUG_ASSERT(penalty >= 0);
  return penalty;
}

static inline void update_hit_prob_cdf(bucket_t *bkt) {
  hitProb_t *hb = bkt->hit_prob;
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

  static int last_overflow = 0;
  if (bkt->hit_prob->n_overflow > 0) {
    if (bkt->hit_prob->n_overflow > last_overflow) {
      printf("bucket %d overflow %ld\n", bkt->bucket_idx, (long) bkt->hit_prob->n_overflow);
      last_overflow = bkt->hit_prob->n_overflow;
    }
    bkt->hit_prob->n_overflow = 0;
  }
}

static inline void print_bucket(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  printf("bucket has segs: ");
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    if (params->buckets[i].n_seg != 0) {
      printf("%d (%d), ", i, params->buckets[i].n_seg);
    }
  }
  printf("\n");
}

static inline void print_seg(cache_t *cache, segment_t *seg, int log_level) {
  LLSC_params_t *params = cache->cache_params;
//  static __thread char msg[1024];

//  log_level,
  printf("seg %6d, age %6d, mean obj size %8.0lf bytes, "
         "req/write rate %6.0lf/%4.2lf, "
         "write ratio/cold miss %.4lf/%.4lf, "
         "mean freq %4.2lf, total hit %6d, total active %4d, "
         "%2d merges, penalty %.4lf, oracle_score %lf, %d obj have reuse, "
         "n_hit/active window %d %d %d %d, \n",
         seg->seg_id, (int) params->curr_rtime - seg->create_rtime,
         (double) seg->total_bytes / seg->n_total_obj, seg->req_rate, seg->write_rate,
         seg->write_ratio, seg->cold_miss_ratio,
         (double) seg->n_total_hit / seg->n_total_obj, seg->n_total_hit, seg->n_total_active,

         seg->n_merge, seg->penalty,
          cal_seg_penalty(cache, OBJ_SCORE_ORACLE,
                          seg, params->n_retain_from_seg,
                          params->curr_rtime, params->curr_vtime),
         count_n_obj_reuse(cache, seg),

         seg->feature.n_hit_per_min[0],
         seg->feature.n_hit_per_min[1],
         seg->feature.n_hit_per_min[2],
         seg->feature.n_hit_per_min[3]
         );

}

//
//  a learned log-structure cache module
//
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"
#include "../include/cacheAlg.h"
#include <assert.h>
#include <stdbool.h>



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
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    bucket->first_seg = segment;
  } else {
    bucket->last_seg->next_seg = segment;
    segment->prev_seg = bucket->last_seg;
  }

  bucket->last_seg = segment;
  segment->next_seg = NULL;
}

static inline segment_t *allocate_new_seg(cache_t *cache, int32_t create_time) {
  LLSC_params_t *params = cache->cache_params;

  /* allocate a new segment */
  segment_t *new_seg = my_malloc(segment_t);
  memset(new_seg, 0, sizeof(segment_t));
  new_seg->objs = my_malloc_n(cache_obj_t, params->segment_size);
  DEBUG_ASSERT(new_seg->objs != NULL);
  memset(new_seg->objs, 0, sizeof(cache_obj_t) * params->segment_size);

  new_seg->next_seg = NULL;
  //  new_seg->first_obj = new_seg->last_obj = NULL;
  new_seg->n_total_obj = new_seg->total_byte = 0;
  new_seg->create_time = create_time;
  new_seg->is_training_seg = false;
  new_seg->magic = MAGIC;
  new_seg->seg_id = params->n_allocated_segs++;

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

static inline void transform_seg_to_training(cache_t *cache, bucket_t *bucket, segment_t *segment) {
  LLSC_params_t *params = cache->cache_params;
  segment->is_training_seg = true;

  /* remove from the bucket */
  remove_seg_from_bucket(bucket, segment);

  /* add to training bucket */
  params->n_segs -= 1;
  params->n_training_segs += 1;

  bucket->n_seg -= 1;
  params->training_bucket.n_seg += 1;

  append_seg_to_bucket(&params->training_bucket, segment);
}

static inline int seg_history_idx(segment_t *segment, int32_t curr_time) {

  return 0;
}

static inline void seg_hit(segment_t *segment, int last_history_idx) {
  ;
}

static inline void clean_one_training_seg(cache_t *cache, segment_t *seg) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj;

  for (int i = 0; i < params->segment_size; i++) {
    cache_obj = &seg->objs[i];
    hashtable_delete(cache->hashtable, cache_obj);
  }
  my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
  my_free(sizeof(segment_t), seg);
}

static void clean_training_segs(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  segment_t *seg = params->training_bucket.first_seg;
  int n_clean = 0;

  while (seg != NULL) {
    segment_t *next_seg = seg->next_seg;
    clean_one_training_seg(cache, seg);
    seg = next_seg;
    n_clean += 1;
  }
  DEBUG_ASSERT(n_clean == params->training_bucket.n_seg);
  params->n_training_segs = 0;
  params->training_bucket.n_seg = 0;
  params->training_bucket.first_seg = NULL;
  params->training_bucket.last_seg = NULL;
}

static void training() {
  ;
}

static void inference() {
  ;
}

static void select_seg(cache_t *cache, segment_t **segs) {
  ;
}

void init_seg_sel(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  params->seg_sel.score_array = my_malloc_n(double, params->n_merge * params->segment_size);
  params->seg_sel.score_array_size = params->n_merge * params->segment_size;

  params->seg_sel.obj_penalty = my_malloc_n(double, params->segment_size);
  params->seg_sel.obj_penalty_array_size = params->segment_size;

  params->seg_sel.last_rank_time = -100000000;
  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;
}

cache_t *LLSC_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LLSC", ccache_params);
  LLSC_init_params_t *LLSC_init_params = init_params;

  LLSC_params_t *params = my_malloc(LLSC_params_t);
  memset(params, 0, sizeof(LLSC_params_t));
  params->curr_evict_bucket_idx = -1;
  params->segment_size = LLSC_init_params->segment_size;
  params->n_merge = LLSC_init_params->n_merge;
  params->last_cache_full_time = -1;
  params->rank_intvl = LLSC_init_params->rank_intvl;
  params->type = LLSC_init_params->type;

  if (params->rank_intvl <= 0) {
    params->rank_intvl = DEFAULT_RANK_INTVL;
  }

  if (params->segs_to_evict == NULL) {
    params->segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
    memset(params->segs_to_evict, 0, sizeof(segment_t *) * params->n_merge);
  }

  cache->hashtable->external_obj = true;
  cache->cache_params = params;
  cache->init_params = init_params;

  init_seg_sel(cache);


  return cache;
}

__attribute__((unused)) void
LLSC_free(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  bucket_t *bkt = &params->training_bucket;
  segment_t *seg = bkt->first_seg, *next_seg;

  while (seg != NULL) {
    next_seg = seg->next_seg;
    my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
    my_free(sizeof(segment_t), seg);
    seg = next_seg;
  }

  for (int i = 0; i < params->n_used_buckets; i++) {
    bkt = &params->buckets[i];
    seg = bkt->first_seg;

    while (seg != NULL) {
      next_seg = seg->next_seg;
      my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
      my_free(sizeof(segment_t), seg);
      seg = next_seg;
    }
  }

  my_free(sizeof(LLSC_params_t), params);
  cache_struct_free(cache);
}

__attribute__((unused)) cache_ck_res_e
LLSC_check(cache_t *cache, request_t *req, bool update_cache) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);
  cache_ck_res_e ret = cache_ck_miss;
  int n_in_cache = 0;

  if (cache_obj != NULL) {
//    printf("%ld %ld %ld %ld/%ld %ld %ld\n", cache->cache_size, params->curr_vtime, req->obj_id, req->obj_size, cache_obj->obj_size, req->next_access_ts, cache_obj->next_access_ts);
    DEBUG_ASSERT(cache_obj->idx_in_segment >= 0);
    DEBUG_ASSERT(cache_obj->idx_in_segment < params->segment_size);
    DEBUG_ASSERT(cache_obj->next_access_ts == params->curr_vtime);

    segment_t *seg = (segment_t *) cache_obj->segment;
    DEBUG_ASSERT(&seg->objs[cache_obj->idx_in_segment] == cache_obj);
  }

  if (!update_cache) {
    while (cache_obj != NULL) {
      if (cache_obj->in_cache) {
        ret = cache_ck_hit;
      }
      cache_obj = cache_obj->hash_next;
      while (cache_obj != NULL && cache_obj->obj_id != req->obj_id) {
        cache_obj = cache_obj->hash_next;
      }
    }

    return ret;
  }

  while (cache_obj != NULL) {
    /* we need to update this object and any possible object in the training segment
     * this object itself can be in the training seg */
    cache_obj->LLSC_freq += 1;
    cache_obj->next_access_ts = req->next_access_ts;

    /* update segment metadata */
    segment_t *curr_seg = cache_obj->segment;
    seg_hit(curr_seg, cache_obj->last_history_idx);
    cache_obj->last_history_idx = seg_history_idx(curr_seg, req->real_time);

    if (cache_obj->in_cache) {
      n_in_cache += 1;
      ret = cache_ck_hit;
      DEBUG_ASSERT(curr_seg->is_training_seg == false);
    } else {
      DEBUG_ASSERT(curr_seg->is_training_seg == true);
    }

    /* find any object on the training seg by checking all objects on the same hash chain */
    cache_obj = cache_obj->hash_next;
    while (cache_obj != NULL && cache_obj->obj_id != req->obj_id) {
      cache_obj = cache_obj->hash_next;
    }
  }
  DEBUG_ASSERT(n_in_cache <= 1);

  return ret;
}

__attribute__((unused)) cache_ck_res_e
LLSC_get(cache_t *cache, request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  params->curr_time = req->real_time;
  params->curr_vtime++;

  cache_ck_res_e ret = cache_get(cache, req);

  return ret;
}

static inline int LLSC_find_bucket_idx(request_t *req) {
  return 0;
}

static int _debug_count_n_obj(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  int64_t n_obj = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    segment_t *curr_seg = params->buckets[i].first_seg;
    int j = 0;
    while (curr_seg) {
      //        printf("bucket %d, seg %d: %d objects training %d\n", i, j, curr_seg->n_total_obj, curr_seg->is_training_seg);
      n_obj += curr_seg->n_total_obj;
      curr_seg = curr_seg->next_seg;
      j++;
    }
  }

  return n_obj;
}

__attribute__((unused)) void
LLSC_insert(cache_t *cache, request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  bucket_t *bucket = &params->buckets[LLSC_find_bucket_idx(req)];
  segment_t *segment = bucket->last_seg;

  if (segment == NULL || segment->n_total_obj == params->segment_size) {
    DEBUG_ASSERT(segment == NULL || segment->next_seg == NULL);
    segment = allocate_new_seg(cache, req->real_time);
    append_seg_to_bucket(bucket, segment);
    params->n_segs += 1;
    bucket->n_seg += 1;
  }

  cache_obj_t *cache_obj = &segment->objs[segment->n_total_obj];
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->LLSC_freq = 0;
  cache_obj->last_access_rtime = req->real_time;
  cache_obj->last_history_idx = -1;
  cache_obj->in_cache = 1;
  cache_obj->segment = segment;
  cache_obj->idx_in_segment = segment->n_total_obj;
  cache_obj->next_access_ts = req->next_access_ts;

  hashtable_insert_obj(cache->hashtable, cache_obj);

  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  segment->total_byte += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
  segment->n_total_obj += 1;

  params->n_byte_written += cache_obj->obj_size;

  //  int64_t n_obj = _debug_count_n_obj(cache);
  //  DEBUG_ASSERT(cache->n_obj == n_obj);

  DEBUG_ASSERT(cache->n_obj > (params->n_segs - 1) * params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_segs * params->segment_size);
}

static inline double cal_object_score(cache_t *cache, cache_obj_t *cache_obj) {
  LLSC_params_t *params = cache->cache_params;
  if (params->type == SEGCACHE || params->type == SEGCACHE_SEG_ORACLE) {
    return (double) (cache_obj->freq + 0.01) / cache_obj->obj_size;
  } else if (params->type == SEGCACHE_ITEM_ORACLE || params->type == SEGCACHE_BOTH_ORACLE || params->type == LOGCACHE_ITEM_ORACLE || params->type == LOGCACHE_BOTH_ORACLE) {
    return (double) 1000000.0 / cache_obj->next_access_ts;
  } else if (params->type == LOGCACHE_LEARNED || params->type == LOGCACHE_LOG_ORACLE) {
    return (double) (cache_obj->freq + 0.01) / cache_obj->obj_size;
    //    return (double) cache_obj->freq * 1000 / (double) (params->curr_time - cache_obj->last_access_rtime) / cache_obj->obj_size;
  } else if (params->type == LOGCACHE_RAMCLOUD) {
    return 1000000.0 / cache_obj->last_access_rtime;
  } else {
    printf("unknown cache type %d\n", params->type);
    abort();
  }
}

static inline int cmp_double(const void *p1, const void *p2) {
  if (*(double *) p1 < *(double *) p2)
    return -1;
  else
    return 1;
}

double find_cutoff(cache_t *cache, segment_t **segs_to_merge, int n_segs) {
  LLSC_params_t *params = cache->cache_params;

  segment_t *seg;
  int pos = 0;

  for (int i = 0; i < n_segs; i++) {
    seg = segs_to_merge[i];
    for (int j = 0; j < seg->n_total_obj; j++) {
      params->seg_sel.score_array[pos++] = cal_object_score(cache, &seg->objs[j]);
    }
  }
  DEBUG_ASSERT(pos == params->seg_sel.score_array_size);
  qsort(params->seg_sel.score_array, pos, sizeof(double), cmp_double);

  return params->seg_sel.score_array[pos - params->segment_size];
}

//static inline bool keep_obj(cache_t *cache, cache_obj_t *cache_obj, double cutoff) {
//  return cal_object_score(cache, cache_obj) >= cutoff;
//}

bool is_seg_evictable_segcache(segment_t *seg, int min_evictable) {
  if (seg == NULL)
    return false;

  int n_evictable = 0;
  while (seg->next_seg != NULL && n_evictable < min_evictable) {
    n_evictable += 1;
    seg = seg->next_seg;
  }
  return n_evictable == min_evictable;
}

void LLSC_merge_segs(cache_t *cache, bucket_t *bucket, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;
  segment_t *new_seg = allocate_new_seg(cache, segs[0]->create_time);
  params->n_segs += 1;
  bucket->n_seg += 1;
  //  printf("among %ld/%ld merge seg %d (%lf) %d (%lf) seg %lf/%lf\n",
  //         params->n_segs, params->n_allocated_segs,
  //         segs[0]->seg_id, segs[0]->penalty, segs[1]->seg_id, segs[1]->penalty,
  //         params->buckets[0].first_seg->penalty, params->buckets[0].first_seg->next_seg->penalty
  //         );

  //  if (params->type == SEGCACHE || params->type == SEGCACHE_ITEM_ORACLE || params->type == SEGCACHE_SEG_ORACLE || params->type == SEGCACHE_BOTH_ORACLE) {
  //    /* insert new seg before the first evicted seg */
  //    link_new_seg_before_seg(bucket, segs[0], new_seg);
  //  } else if (params->type == LOGCACHE_FIFO || params->type == LOGCACHE_RAMCLOUD ||
  //             params->type == LOGCACHE_ITEM_ORACLE || params->type == LOGCACHE_LOG_ORACLE ||
  //             params->type == LOGCACHE_BOTH_ORACLE || params->type == LOGCACHE_LEARNED) {
  //    append_seg_to_bucket(bucket, new_seg);
  //  }

  link_new_seg_before_seg(bucket, segs[0], new_seg);

  cache_obj_t *cache_obj;
  double cutoff = find_cutoff(cache, segs, params->n_merge);
  int64_t n_copied = 0, n_copied_byte = 0;

  int n_reuse = 0;
  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(segs[i]->magic == MAGIC);
    for (int j = 0; j < MIN(params->segment_size, segs[i]->n_total_obj); j++) {
      cache_obj = &segs[i]->objs[j];
      if (cache_obj->next_access_ts > 0)
        n_reuse += 1;
      if (n_copied < params->segment_size && cal_object_score(cache, cache_obj) >= cutoff) {
        cache_obj_t *new_obj = &new_seg->objs[n_copied];
        memcpy(new_obj, cache_obj, sizeof(cache_obj_t));
        new_obj->LLSC_freq = (new_obj->LLSC_freq + 1) / 2;
        new_obj->idx_in_segment = n_copied;
        new_obj->segment = new_seg;
        hashtable_insert_obj(cache->hashtable, new_obj);

        n_copied += 1;
        n_copied_byte += cache_obj->obj_size;
      } else {
        cache->n_obj -= 1;
        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
      }
      cache_obj->in_cache = 0;
    }
    transform_seg_to_training(cache, bucket, segs[i]);
  }

  /* not sure if using oracle info */
  //  DEBUG_ASSERT(n_copied == params->segment_size);
  new_seg->n_total_obj = n_copied;
  new_seg->total_byte = n_copied_byte;
  if (new_seg->n_total_obj < params->segment_size)
    printf("%d objects copied\n", new_seg->n_total_obj);

  for (int i = 0; i < params->n_merge; i++) {
    for (int j = 0; j < MIN(params->segment_size, segs[i]->n_total_obj); j++) {
      DEBUG_ASSERT(segs[i]->objs[j].in_cache == 0);
      DEBUG_ASSERT(segs[i]->objs[j].idx_in_segment == j);
    }
  }

  //  printf("merge seg %ld %ld\n", segs[0]->seg_id, segs[1]->seg_id);
}

static void cal_seg_evict_penalty_oracle(cache_t *cache, segment_t *seg, int curr_vtime) {
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

static double cal_seg_evict_penalty_learned(cache_t *cache, segment_t *seg) {
  ;
}

static double cal_seg_evict_penalty_ramcloud(segment_t *seg, int64_t curr_time) {
  /* this is not real ramcloud */
  abort();
  return 1.0 / (double) (curr_time - seg->create_time);
}

static bucket_t *select_segs_segcache(cache_t *cache, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;

  if (params->curr_evict_bucket_idx == -1)
    params->curr_evict_bucket_idx = 0;
  bucket_t *bucket = &params->buckets[params->curr_evict_bucket_idx];
  segment_t *seg_to_evict = params->next_evict_segment;

  while (!is_seg_evictable_segcache(seg_to_evict, params->n_merge)) {
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];
    seg_to_evict = bucket->first_seg;
  }

  for (int i = 0; i < params->n_merge; i++) {
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }
  params->next_evict_segment = seg_to_evict;

  return bucket;
}

static inline int cmp_seg_segoracle(const void *p1, const void *p2) {
  segment_t *seg1 = (segment_t *) p1;
  segment_t *seg2 = (segment_t *) p2;

  abort();
  return 0;
}

static inline int cmp_seg_logoracle(const void *p1, const void *p2) {
  segment_t *seg1 = *(segment_t **) p1;
  segment_t *seg2 = *(segment_t **) p2;

  DEBUG_ASSERT(seg1->magic == MAGIC);

  if (seg1->penalty < seg2->penalty)
    return -1;
  else
    return 1;
}

static bucket_t *select_segs(cache_t *cache, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;
  bool array_resized = false;

  if (params->type == SEGCACHE || params->type == SEGCACHE_ITEM_ORACLE || params->type == LOGCACHE_ITEM_ORACLE || params->type == LOGCACHE_FIFO)
    return select_segs_segcache(cache, segs);

  /* setup function */
  if (params->seg_sel.ranked_seg_size < params->n_segs) {
    if (params->seg_sel.ranked_segs != NULL)
      my_free(sizeof(segment_t *) * ranked_seg_size, params->seg_sel.ranked_segs);
    params->seg_sel.ranked_segs = my_malloc_n(segment_t *, params->n_segs * 2);
    params->seg_sel.ranked_seg_size = params->n_segs * 2;
    array_resized = true;
  }

  if (params->n_evictions - params->seg_sel.last_rank_time > params->rank_intvl || array_resized) {
    int n_seg = 0;
    segment_t *curr_seg;
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      curr_seg = params->buckets[i].first_seg;
      while (curr_seg) {
        if (params->type == LOGCACHE_LEARNED) {
          ;
        } else if (params->type == SEGCACHE_BOTH_ORACLE || params->type == SEGCACHE_SEG_ORACLE || params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE) {
          cal_seg_evict_penalty_oracle(cache, curr_seg, params->curr_vtime);
        } else if (params->type == LOGCACHE_RAMCLOUD) {
          cal_seg_evict_penalty_ramcloud(curr_seg, params->curr_time);
        }
        params->seg_sel.ranked_segs[n_seg++] = curr_seg;
        curr_seg = curr_seg->next_seg;
      }
    }
    DEBUG_ASSERT(n_seg == params->n_segs);

    if (params->type == SEGCACHE_SEG_ORACLE || params->type == SEGCACHE_BOTH_ORACLE) {
      qsort(params->seg_sel.ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_segoracle);
    } else if (params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE || params->type == LOGCACHE_LEARNED || params->type == LOGCACHE_RAMCLOUD) {
      qsort(params->seg_sel.ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_logoracle);
    } else {
      abort();
    }

    //    printf("seg penalty %lf@%d %lf@%d %lf@%d %lf@%d %lf@%d %lf@%d %lf@%d %lf@%d\n",
    //           ranked_segs[0]->penalty, 0, ranked_segs[10]->penalty, 10,
    //           ranked_segs[1]->penalty, 1, ranked_segs[2]->penalty, 2,
    //           ranked_segs[6]->penalty, 6, ranked_segs[7]->penalty, 7,
    //           ranked_segs[params->n_segs-20]->penalty, -20, ranked_segs[params->n_segs - 1]->penalty, -1
    //    );

    params->seg_sel.last_rank_time = params->n_evictions;
    params->seg_sel.ranked_seg_pos = 0;
  }

  //  printf("%d %d %d %p\n", params->n_evictions, last_rank_time, ranked_seg_pos, ranked_segs[6]);

  if (params->seg_sel.ranked_seg_pos > params->n_segs / 2) {
    WARNING("rank frequency too low, curr pos in ranked seg %d, total %ld segs\n", params->seg_sel.ranked_seg_pos, (long) params->n_segs);
  }

  if (params->type > LOGCACHE_START_POS) {
    for (int i = 0; i < params->n_merge; i++) {
      //      printf("set %d to %p and pos %d to NULL\n", i, ranked_segs[ranked_seg_pos], ranked_seg_pos);
      segs[i] = params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos];
      DEBUG_ASSERT(segs[i] != NULL);
      params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos] = NULL;
      params->seg_sel.ranked_seg_pos++;
    }
    //    printf("%d %d %d %p\n", params->n_evictions, last_rank_time, ranked_seg_pos, ranked_segs[6]);

    return &params->buckets[0];
  } else {
    /* it is non-trivial to determine the bucket of segcache */
    segs[0] = params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos];
    params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos] = NULL;
    params->seg_sel.ranked_seg_pos++;

    printf("do not support\n");
    abort();
  }
}

void LLSC_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  LLSC_params_t *params = cache->cache_params;

  bucket_t *bucket = select_segs(cache, params->segs_to_evict);
  for (int i = 0; i < params->n_merge; i++)
    DEBUG_ASSERT(params->segs_to_evict[i] != NULL);

  LLSC_merge_segs(cache, bucket, params->segs_to_evict);

  /* discover time to write cache_size bytes and the time_window in history recording */
  if (params->n_byte_written >= cache->cache_size) {
    if (params->last_cache_full_time == -1) {
      params->last_cache_full_time = req->real_time;
    } else {
      int32_t time_write_full_cache = req->real_time - params->last_cache_full_time;
      params->full_cache_write_time = time_write_full_cache;
      params->time_window = params->full_cache_write_time / N_TIME_WINDOW;
      params->start_feature_recording = true;
    }
  }

  //  printf("cache size %lu/%lu, %ld segments, %ld training segments\n",
  //         (unsigned long) cache->occupied_size, (unsigned long) cache->cache_size,
  //         (long) params->n_segs, (long) params->n_training_segs
  //         );

  params->n_evictions += 1;

  if (params->n_training_segs > params->n_segs * 2)
    clean_training_segs(cache);
}

void LLSC_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  LLSC_params_t *params = cache->cache_params;
  abort();

  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail,
                       cache_obj);
  hashtable_delete(cache->hashtable, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;
}

void LLSC_remove(cache_t *cache, obj_id_t obj_id) {
  abort();

  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;

  hashtable_delete(cache->hashtable, cache_obj);
}

#ifdef __cplusplus
}
#endif

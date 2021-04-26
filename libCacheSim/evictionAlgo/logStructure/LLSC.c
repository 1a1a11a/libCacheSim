//
//  a learned log-structure cache module
//
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"
#include "learned.h"
#include "log.h"
#include "oracle.h"

#include <assert.h>
#include <stdbool.h>

void init_seg_sel(LLSC_params_t *params) {
  params->seg_sel.score_array = my_malloc_n(double, params->n_merge * params->segment_size);
  params->seg_sel.score_array_size = params->n_merge * params->segment_size;

  //  params->seg_sel.obj_penalty = my_malloc_n(double, params->segment_size);
  //  params->seg_sel.obj_penalty_array_size = params->segment_size;

  params->seg_sel.last_rank_time = -INT32_MAX;
  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;
}

void init_learner(LLSC_params_t *params) {

  params->learner.last_cache_full_time = -1;
  //  params->learner.start_feature_recording = false;
  //  params->learner.start_feature_recording_time = INT64_MAX;
  //  params->learner.feature_history_time_window = 0;
  params->learner.n_byte_written = 0;
  params->learner.n_feature = N_FEATURE_TIME_WINDOW * 6 + 11;

  //  params->learner.feature_history_time_window = 60;
  //  params->learner.start_feature_recording = true;
  //  params->learner.start_feature_recording_time = INT32_MAX;

  //  params->learner.start_train = false;

  params->learner.pred = NULL;
  params->learner.training_x = NULL;
  params->learner.training_n_row = 0;
  params->learner.validation_n_row = 0;

  params->learner.inference_data = NULL;
  params->learner.inference_n_row = 0;

  //  params->learner.n_training_iter = 0;
  params->learner.n_train = 0;
  params->learner.n_inference = 0;
}

static void init_buckets(LLSC_params_t *params) {
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    params->buckets[i].bucket_idx = i;
    for (int j = 0; j < HIT_PROB_MAX_AGE; j++) {
      /* initialize to a small number, when the hit density is not available
       * before eviction, we use size to make eviction decision */
      params->buckets[i].hit_prob.hit_density[j] = 1e-8;
    }
  }
}

cache_t *LLSC_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LLSC", ccache_params);
  LLSC_init_params_t *LLSC_init_params = init_params;

  LLSC_params_t *params = my_malloc(LLSC_params_t);
  memset(params, 0, sizeof(LLSC_params_t));
  params->curr_evict_bucket_idx = -1;
  params->segment_size = LLSC_init_params->segment_size;
  params->n_merge = LLSC_init_params->n_merge;
  params->n_retain_from_seg = params->segment_size / params->n_merge;
  params->rank_intvl = LLSC_init_params->rank_intvl;
  params->type = LLSC_init_params->type;
  params->bucket_type = LLSC_init_params->bucket_type;

  if (params->rank_intvl <= 0) {
    params->rank_intvl = DEFAULT_RANK_INTVL;
  }

  switch (params->type) {
    case SEGCACHE:
    case SEGCACHE_SEG_ORACLE:
    case LOGCACHE_LOG_ORACLE:
    case LOGCACHE_LEARNED:
      //                  params->obj_score_type = OBJ_SCORE_FREQ_BYTE;
      params->obj_score_type = OBJ_SCORE_HIT_DENSITY;
      break;
    case SEGCACHE_ITEM_ORACLE:
    case SEGCACHE_BOTH_ORACLE:
    case LOGCACHE_ITEM_ORACLE:
    case LOGCACHE_BOTH_ORACLE: params->obj_score_type = OBJ_SCORE_ORACLE; break;
    default: abort();
  };

  init_seg_sel(params);
  init_learner(params);
  init_buckets(params);

  if (params->segs_to_evict == NULL) {
    params->segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
    memset(params->segs_to_evict, 0, sizeof(segment_t *) * params->n_merge);
  }

  cache->hashtable->external_obj = true;
  cache->cache_params = params;
  cache->init_params = init_params;

  return cache;
}

__attribute__((unused)) void LLSC_free(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  bucket_t *bkt = &params->training_bucket;
  segment_t *seg = bkt->first_seg, *next_seg;

  while (seg != NULL) {
    next_seg = seg->next_seg;
    my_free(sizeof(cache_obj_t) * params->segment_size, seg->objs);
    my_free(sizeof(segment_t), seg);
    seg = next_seg;
  }

  for (int i = 0; i < MAX_N_BUCKET; i++) {
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

#ifdef TRAINING_TRUTH_ONLINE
__attribute__((unused)) cache_ck_res_e LLSC_check(cache_t *cache, request_t *req,
                                                  bool update_cache) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    assert(0);
  }

  if (cache_obj->LSC.in_cache) {
    /* if there is an evicted entry, the evicted entry should be after this entry */
    DEBUG_ASSERT(((segment_t *) (cache_obj->LSC.segment))->is_training_seg == false);

    seg_hit(params, cache_obj);
    object_hit(params, cache_obj, req);

    cache_obj = cache_obj->hash_next;
    while (cache_obj && cache_obj->obj_id != req->obj_id) {
      cache_obj = cache_obj->hash_next;
    }

    while (cache_obj) {
      /* history entry, it can have more than one history entry, when an object
       * is retained multiple times then get a hit,
       * we can delete the history upon the second merge, or
       * we can remove it here */
      DEBUG_ASSERT(cache_obj->LSC.in_cache == 0);
      segment_t *seg = cache_obj->LSC.segment;
      seg->penalty += 1.0e6 / (double) (params->curr_vtime - seg->eviction_vtime) / cache_obj->obj_size;
      DEBUG_ASSERT(seg->is_training_seg == true);
      hashtable_delete(cache->hashtable, cache_obj);

      cache_obj = cache_obj->hash_next;
      while (cache_obj && cache_obj->obj_id != req->obj_id) {
        cache_obj = cache_obj->hash_next;
      }
    }

    return cache_ck_hit;

  } else {
    /* no entry in cache */
    while (cache_obj) {
      DEBUG_ASSERT(cache_obj->LSC.in_cache == 0);
      segment_t *seg = cache_obj->LSC.segment;
      seg->penalty += 1.0e6 / (double) (params->curr_vtime - seg->eviction_vtime) / cache_obj->obj_size;
      DEBUG_ASSERT(seg->is_training_seg == true);
      hashtable_delete(cache->hashtable, cache_obj);

      /* there cound be more than one history entry if the object is retained several times and evicted
       * we can delete the history upon the second merge, or we can remove it here */
      cache_obj = cache_obj->hash_next;
      while (cache_obj && cache_obj->obj_id != req->obj_id) {
        cache_obj = cache_obj->hash_next;
      }
    }

    return cache_ck_miss;
  }
}
#else
__attribute__((unused)) cache_ck_res_e LLSC_check(cache_t *cache, request_t *req,
                                                  bool update_cache) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    return cache_ck_hit;
  }

  DEBUG_ASSERT(cache_obj->LSC.in_cache);
  DEBUG_ASSERT(((segment_t *) (cache_obj->LSC.segment))->is_training_seg == false);

  seg_hit(params, cache_obj);
  object_hit(params, cache_obj, req);


  return cache_ck_hit;
}
#endif

__attribute__((unused)) cache_ck_res_e LLSC_get(cache_t *cache, request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  params->curr_rtime = req->real_time;
  params->curr_vtime++;

  cache_ck_res_e ret = cache_get(cache, req);

  params->n_req_since_last_eviction += 1;
  if (ret == cache_ck_miss) params->n_miss_since_last_eviction += 1;

  return ret;
}

__attribute__((unused)) void LLSC_insert(cache_t *cache, request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  bucket_t *bucket = &params->buckets[find_bucket_idx(params, req)];
  segment_t *segment = bucket->last_seg;

  if (segment == NULL || segment->n_total_obj == params->segment_size) {
    DEBUG_ASSERT(segment == NULL || segment->next_seg == NULL);
    if (segment != NULL) {
      double rtime = ((double) params->curr_rtime - segment->create_rtime + 1);
      double n_req = (double) (params->curr_vtime - segment->create_vtime);
      segment->req_rate = n_req / rtime;
      segment->write_rate = (double) segment->n_total_obj / rtime;
      segment->write_ratio = (double) segment->n_total_obj / n_req;
      segment->cold_miss_ratio =
          (double) params->n_miss_since_last_eviction / params->n_req_since_last_eviction;
      //      printf("set req rate write rate %lf %lf\n", segment->req_rate, segment->write_rate);
    }

    segment = allocate_new_seg(cache, bucket->bucket_idx);
    append_seg_to_bucket(params, bucket, segment);
  }

  cache_obj_t *cache_obj = &segment->objs[segment->n_total_obj];
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->LSC.LLSC_freq = 0;
  cache_obj->LSC.last_access_rtime = req->real_time;
  //  cache_obj->LSC.last_history_idx = -1;
  cache_obj->LSC.in_cache = 1;
  cache_obj->LSC.segment = segment;
  cache_obj->LSC.idx_in_segment = segment->n_total_obj;
  cache_obj->LSC.next_access_ts = req->next_access_ts;

  hashtable_insert_obj(cache->hashtable, cache_obj);

  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  segment->total_byte += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
  segment->n_total_obj += 1;

  params->learner.n_byte_written += cache_obj->obj_size;

  DEBUG_ASSERT(cache->n_obj > (params->n_segs - params->n_used_buckets) * params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_segs * params->segment_size);
}

static inline int count_hash_chain_len(cache_obj_t *cache_obj) {
  int n = 0;
  while (cache_obj) {
    n += 1;
    cache_obj = cache_obj->hash_next;
  }
  return n;
}

void LLSC_merge_segs(cache_t *cache, bucket_t *bucket, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;

  DEBUG_ASSERT(bucket->bucket_idx == segs[0]->bucket_idx);
  DEBUG_ASSERT(bucket->bucket_idx == segs[1]->bucket_idx);

  segment_t *new_seg = allocate_new_seg(cache, bucket->bucket_idx);
  new_seg->create_rtime = segs[0]->create_rtime;
  new_seg->create_vtime = segs[0]->create_vtime;
  double req_rate = 0, write_rate = 0;
  int n_merge = 0;
  for (int i = 0; i < params->n_merge; i++) {
    req_rate += segs[i]->req_rate;
    write_rate += segs[i]->write_rate;
    n_merge = n_merge < segs[i]->n_merge ? segs[i]->n_merge : n_merge;
  }
  new_seg->req_rate = req_rate;
  new_seg->write_rate = write_rate;
  new_seg->n_merge = n_merge + 1;

  //  if (params->type > LOGCACHE_START_POS) {
  //    append_seg_to_bucket_before_last(bucket, new_seg);
  //  } else {
  link_new_seg_before_seg(params, bucket, segs[0], new_seg);
  //  }

  //  print_seg(cache, segs[0]);
  //  print_seg(cache, segs[1]);

  cache_obj_t *cache_obj;
  double cutoff =
      find_cutoff(cache, params->obj_score_type, segs, params->n_merge, params->segment_size);

  int n_reuse = 0;
  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(segs[i]->magic == MAGIC);
    for (int j = 0; j < segs[i]->n_total_obj; j++) {
      cache_obj = &segs[i]->objs[j];
      if (cache_obj->LSC.next_access_ts > 0) n_reuse += 1;
      if (new_seg->n_total_obj < params->segment_size
          && cal_object_score(params, params->obj_score_type, cache_obj, params->curr_rtime,
                              params->curr_vtime)
                 >= cutoff) {
        cache_obj_t *new_obj = &new_seg->objs[new_seg->n_total_obj];
        memcpy(new_obj, cache_obj, sizeof(cache_obj_t));
        new_obj->LSC.LLSC_freq = (new_obj->LSC.LLSC_freq + 1) / 2;
        new_obj->LSC.idx_in_segment = new_seg->n_total_obj;
        new_obj->LSC.segment = new_seg;
        new_obj->LSC.n_merged += 1;
        hashtable_insert_obj(cache->hashtable, new_obj);

        new_seg->n_total_obj += 1;
        new_seg->total_byte += cache_obj->obj_size;

//        hashtable_delete(cache->hashtable, cache_obj);
      } else {
        cache->n_obj -= 1;
        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
      }
      object_evict(cache, cache_obj);
      cache_obj->LSC.in_cache = 0;
    }

    if (params->type == LOGCACHE_LEARNED
        && rand() % GEN_TRAINING_SEG_EVERY_N == 0) {
      transform_seg_to_training(cache, bucket, segs[i]);
    } else {
      remove_seg_from_bucket(params, bucket, segs[i]);
      clean_one_seg(cache, segs[i]);
    }
  }

  /* not sure if using oracle info */
  if (new_seg->n_total_obj < params->segment_size) {
    WARNING("cutoff %lf %d objects copied %d reuse\n", cutoff, new_seg->n_total_obj, n_reuse);
    abort();
  }
}

static bucket_t *select_segs_segcache(cache_t *cache, segment_t **segs) {
  LLSC_params_t *params = cache->cache_params;

  if (params->curr_evict_bucket_idx == -1) params->curr_evict_bucket_idx = 0;
  bucket_t *bucket = &params->buckets[params->curr_evict_bucket_idx];
  segment_t *seg_to_evict = params->next_evict_segment;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];
    seg_to_evict = bucket->first_seg;
    n_checked_seg += 1;
    DEBUG_ASSERT(n_checked_seg <= params->n_segs);
  }

  for (int i = 0; i < params->n_merge; i++) {
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }
  params->next_evict_segment = seg_to_evict;

  return bucket;
}

static inline int cmp_seg_seg(const void *p1, const void *p2) {
  segment_t *seg1 = (segment_t *) p1;
  segment_t *seg2 = (segment_t *) p2;

  abort();
  return 0;
}

static inline int cmp_seg_log(const void *p1, const void *p2) {
  segment_t *seg1 = *(segment_t **) p1;
  segment_t *seg2 = *(segment_t **) p2;

  DEBUG_ASSERT(seg1->magic == MAGIC);

  if (seg1->penalty < seg2->penalty) return -1;
  else
    return 1;
}

static bucket_t *select_segs(cache_t *cache, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;
  bool array_resized = false;

  if (params->type == SEGCACHE || params->type == SEGCACHE_ITEM_ORACLE
      || params->type == LOGCACHE_ITEM_ORACLE)
    return select_segs_segcache(cache, segs);

  if (params->type == LOGCACHE_LEARNED && params->learner.n_train == 0)
    return select_segs_segcache(cache, segs);

  /* setup function */
  if (params->seg_sel.ranked_seg_size < params->n_segs) {
    if (params->seg_sel.ranked_segs != NULL) {
      my_free(sizeof(segment_t *) * ranked_seg_size, params->seg_sel.ranked_segs);
    }
    params->seg_sel.ranked_segs = my_malloc_n(segment_t *, params->n_segs * 2);
    params->seg_sel.ranked_seg_size = params->n_segs * 2;
    array_resized = true;
  }

  segment_t **ranked_segs = params->seg_sel.ranked_segs;
  int32_t *ranked_seg_pos = &(params->seg_sel.ranked_seg_pos);

  if (params->n_evictions - params->seg_sel.last_rank_time > params->rank_intvl
      || array_resized) {
    if (params->type == LOGCACHE_LEARNED) {
      inference(cache);
    }

    int n_seg = 0;
    segment_t *curr_seg;
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      curr_seg = params->buckets[i].first_seg;
      while (curr_seg) {
        if (!is_seg_evictable_fifo(curr_seg, 1)) {
          curr_seg->penalty = INT32_MAX;
        } else {
          if (params->type == LOGCACHE_LEARNED) {
            DEBUG_ASSERT(curr_seg->penalty == params->learner.pred[n_seg]);
          } else if (params->type == LOGCACHE_LOG_ORACLE
                     || params->type == SEGCACHE_SEG_ORACLE) {
            curr_seg->penalty = cal_seg_penalty(cache, params->obj_score_type, curr_seg,
                                                params->n_retain_from_seg, params->curr_rtime,
                                                params->curr_vtime);
          } else if (params->type == SEGCACHE_BOTH_ORACLE
                     || params->type == LOGCACHE_BOTH_ORACLE) {
            curr_seg->penalty =
                cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, params->n_retain_from_seg,
                                params->curr_rtime, params->curr_vtime);
          } else {
            abort();
          }
        }
        ranked_segs[n_seg++] = curr_seg;
        curr_seg = curr_seg->next_seg;
      }
    }
    DEBUG_ASSERT(n_seg == params->n_segs);

    if (params->type == SEGCACHE_SEG_ORACLE || params->type == SEGCACHE_BOTH_ORACLE) {
      qsort(ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_seg);
    } else if (params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE
               || params->type == LOGCACHE_LEARNED) {
      qsort(ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_log);
    } else {
      abort();
    }

#if defined(mydebug)
    static int n = 0, n_zero = 0;
    for (int i = 0; i < params->n_segs; i++) {
      n += 1;
      if (ranked_segs[i]->penalty < 1e-6) {
        n_zero += 1;
        if (n_zero % 20 == 0) printf("%d/%d\n", n_zero, n);
      }
    }

    int n_zeros = 0;
    for (int i = 0; i < params->n_segs; i++) {
      if (ranked_segs[i]->penalty < 1e-6) {
        n_zeros += 1;
      }
    }
    printf("cache size %lu, n_segs %d, seg penalty %d zero, %lf@%d %lf@%d %lf@%d %lf@%d %lf@%d "
           "%lf@%d %lf@%d %lf@%d\n",
           (unsigned long) cache->cache_size, params->n_segs, n_zeros, ranked_segs[0]->penalty,
           0, ranked_segs[2]->penalty, 2, ranked_segs[4]->penalty, 4, ranked_segs[8]->penalty,
           8, ranked_segs[16]->penalty, 16, ranked_segs[32]->penalty, 32,
           ranked_segs[params->n_segs - 20]->penalty, -20,
           ranked_segs[params->n_segs - 1]->penalty, -1);
#endif

    params->seg_sel.last_rank_time = params->n_evictions;
    *ranked_seg_pos = 0;
  }

  if (*ranked_seg_pos > params->n_segs / 4) {
    params->rank_intvl /= 2 + 1;
    WARNING("cache size %lu: rank frequency too low, "
            "curr pos in ranked seg %d, total %ld segs, reduce rank_intvl to %d\n",
            (unsigned long) cache->cache_size, *ranked_seg_pos, (long) params->n_segs, params->rank_intvl);
    print_bucket(cache);
    //    params->seg_sel.last_rank_time = 0;
    return select_segs(cache, segs);
  }

  if (params->type > LOGCACHE_START_POS) {
    assert(params->n_merge == 2);
    int i = 0, j = 0;
    segs[i++] = ranked_segs[*ranked_seg_pos];
    ranked_segs[(*ranked_seg_pos)++] = NULL;
    while (ranked_segs[*ranked_seg_pos] == NULL) {
      (*ranked_seg_pos)++;
    }

    while (i < params->n_merge && *ranked_seg_pos + j < params->n_segs / 2) {
      int bucket_idx = segs[0]->bucket_idx;
      while (ranked_segs[*ranked_seg_pos + j] == NULL
             || ranked_segs[*ranked_seg_pos + j]->bucket_idx != bucket_idx) {
        j += 1;
        if (*ranked_seg_pos + j >= params->n_segs / 2) {
          break;
        }
      }

      if (ranked_segs[*ranked_seg_pos + j] != NULL
          && ranked_segs[*ranked_seg_pos + j]->bucket_idx == bucket_idx) {
        /* we find two segments from the same bucket */
        segs[i++] = ranked_segs[*ranked_seg_pos + j];
        ranked_segs[*ranked_seg_pos + j] = NULL;
        (*ranked_seg_pos)++;
        while (ranked_segs[*ranked_seg_pos] == NULL) {
          (*ranked_seg_pos)++;
        }
        break;
      }

      /* we have reached the end of the seg */
      DEBUG_ASSERT(*ranked_seg_pos + j >= params->n_segs / 2);

      /* we cannot find one more segment with the same bucket id,
         * we should try the next segment from ranked_seg_pos */
      i = 0;
      j = 0;
      segs[i++] = ranked_segs[*ranked_seg_pos];
      ranked_segs[(*ranked_seg_pos)++] = NULL;
      while (ranked_segs[*ranked_seg_pos] == NULL) {
        (*ranked_seg_pos)++;
      }
    }
    if (*ranked_seg_pos >= params->n_segs / 2) {
      printf("cache size %ld, %d segs, hard find a matching seg, "
             "please increase cache size or reduce segment size\n",
             (long) cache->cache_size, params->n_segs);
      print_bucket(cache);
      printf("current ranked pos %d\n", *ranked_seg_pos);
      for (int m = 0; m < params->n_segs; m++) {
        if (ranked_segs[m]) printf("seg %d penalty %lf\n", m, ranked_segs[m]->penalty);
        else
          printf("seg %d NULL\n", m);
      }
      //      abort();
      params->seg_sel.last_rank_time = 0;
      //      return select_segs(cache, segs);
    }

    DEBUG_ASSERT(i == params->n_merge);

    return &params->buckets[segs[0]->bucket_idx];
  } else {
    /* it is non-trivial to determine the bucket of segcache */
    segs[0] = params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos];
    params->seg_sel.ranked_segs[params->seg_sel.ranked_seg_pos] = NULL;
    params->seg_sel.ranked_seg_pos++;

    ERROR("do not support\n");
    abort();
  }
}

void LLSC_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  LLSC_params_t *params = cache->cache_params;

  bucket_t *bucket = select_segs(cache, params->segs_to_evict);

  LLSC_merge_segs(cache, bucket, params->segs_to_evict);

  /* discover time to write cache_size bytes and the time_window in history recording */
  //  if (!params->learner.start_feature_recording) {
  //    if (params->learner.n_byte_written >= cache->cache_size * 2) {
  //      if (params->learner.last_cache_full_time == -1) {
  //        params->learner.last_cache_full_time = req->real_time;
  //        params->learner.n_byte_written = 0;
  //      } else {
  //        int32_t time_write_full_cache =
  //            (req->real_time - params->learner.last_cache_full_time) / 2;
  //        params->learner.full_cache_write_time = time_write_full_cache;
  //        params->learner.feature_history_time_window =
  //            params->learner.full_cache_write_time * 2 / N_FEATURE_TIME_WINDOW;
  //        printf("cache %lu set time window to %d, %d segments\n",
  //               (unsigned long) (cache->cache_size / MiB),
  //               params->learner.feature_history_time_window, params->n_segs);
  //        params->learner.start_feature_recording = true;
  //        params->learner.n_byte_written = 0;
  //        params->learner.start_feature_recording_time = req->real_time;
  //      }
  //    }
  //  }

  //  if (params->learner.start_feature_recording_time == INT32_MAX
  //      && params->learner.n_byte_written > cache->cache_size * 2) {
  //    params->learner.start_feature_recording_time = req->real_time;
  //    params->learner.n_byte_written = 0;
  //    printf("cache size %ld %d segs, start feature recording time %ld %ld window %d\n",
  //           (long) cache->cache_size, params->n_segs, (long) params->curr_rtime,
  //           (long) params->curr_vtime, params->learner.feature_history_time_window);
  //  }

  params->n_evictions += 1;
  params->n_req_since_last_eviction = 0;
  params->n_miss_since_last_eviction = 0;

  if (params->type == LOGCACHE_LEARNED) {
    //        if (params->n_training_segs >= params->n_segs * 4) {
    if (params->n_training_segs >= 1000 * params->learner.n_train + 1000) {
      //    if (params->learner.n_byte_written >= cache->cache_size * 2
      //      && params->n_training_segs >= 2000
      //        && params->learner.n_train == 0) {
      train(cache);
    }
  }

  if (params->curr_vtime - params->last_hit_prob_compute_vtime > HIT_PROB_COMPUTE_INTVL) {
    /* update hit prob for all buckets */
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      update_hit_prob_cdf(&params->buckets[i]);
    }
    params->last_hit_prob_compute_vtime = params->curr_vtime;

    //    static int last_print = 0;
    //    if (params->curr_rtime - last_print >= 3600) {
    //      printf("cache size %lu ", (unsigned long) cache->cache_size);
    //      print_bucket(cache);
    //      last_print = params->curr_rtime;
    //    }
  }
}

void LLSC_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  LLSC_params_t *params = cache->cache_params;
  abort();

  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);
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

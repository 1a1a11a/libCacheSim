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
#include "utils.h"
#include "merge.h"

#include <assert.h>
#include <stdbool.h>

static char *LSC_type_names[] = {
    "SEGCACHE", "SEGCACHE_ITEM_ORACLE", "SEGCACHE_SEG_ORACLE",
    "SEGCACHE_BOTH_ORACLE",
    "LOGCACHE_START_POS",
    "LOGCACHE_BOTH_ORACLE", "LOGCACHE_LOG_ORACLE", "LOGCACHE_ITEM_ORACLE",
    "LOGCACHE_LEARNED"
};

static char *obj_score_type_names[] = {
    "OBJ_SCORE_FREQ", "OBJ_SCORE_FREQ_BYTE", "OBJ_SCORE_FREQ_BYTE_AGE",
    "OBJ_SCORE_HIT_DENSITY",
    "OBJ_SCORE_ORACLE"
};

static char *bucket_type_names[] = {
    "NO_BUCKET",
    "SIZE_BUCKET",
    "TTL_BUCKET",
    "CUSTOMER_BUCKET",
    "BUCKET_ID_BUCKET",
    "CONTENT_TYPE_BUCKET"
};

void init_seg_sel(LLSC_params_t *params) {
  params->seg_sel.score_array =
      my_malloc_n(double, params->n_merge * params->segment_size);
  params->seg_sel.score_array_size = params->n_merge * params->segment_size;

  //  params->seg_sel.obj_penalty = my_malloc_n(double, params->segment_size);
  //  params->seg_sel.obj_penalty_array_size = params->segment_size;

  params->seg_sel.last_rank_time = -INT32_MAX;
  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;
}

void init_learner(cache_t *cache, LLSC_params_t *params, LLSC_init_params_t *init_params) {

  learner_t *l = &params->learner;

  //  l->start_feature_recording = false;
  //  l->start_feature_recording_time = INT64_MAX;
  //  l->feature_history_time_window = 0;
//  l->n_byte_written = 0;
  l->n_feature = N_FEATURE_TIME_WINDOW * 3 + 12;

  //  l->feature_history_time_window = 60;
  //  l->start_feature_recording = true;
  //  l->start_feature_recording_time = INT32_MAX;

  //  l->start_train = false;

  l->pred = NULL;
  l->training_x = NULL;
  l->training_n_row = 0;
  l->validation_n_row = 0;

  l->inference_data = NULL;
  l->inference_n_row = 0;

  //  l->n_training_iter = 0;
  l->n_train = 0;
  l->n_inference = 0;

//  l->min_start_train_seg = init_params->min_start_train_seg;
//  l->max_start_train_seg = init_params->max_start_train_seg;
//  l->n_train_seg_growth = init_params->n_train_seg_growth;
//  l->sample_every_n_seg_for_training =
//      init_params->sample_every_n_seg_for_training;
  l->n_segs_to_start_training = 1024*8;
  l->re_train_intvl = init_params->re_train_intvl;
  l->n_evicted_bytes = 0;
  l->n_bytes_start_collect_train = cache->cache_size / 1;
  l->last_train_rtime = 0;

//  if (l->min_start_train_seg <= 0)
//    l->min_start_train_seg = 2000;
//  if (l->max_start_train_seg <= 0)
//    l->max_start_train_seg = 10000;
//  if (l->n_train_seg_growth <= 0)
//    l->n_train_seg_growth = 2000;
  if (l->re_train_intvl <= 0)
    l->re_train_intvl = 86400;

//  l->next_n_train_seg = l->min_start_train_seg;
  if (l->sample_every_n_seg_for_training <= 0)
    l->sample_every_n_seg_for_training = 1;
}

static void init_buckets(LLSC_params_t *params, int age_shift) {
  if (age_shift <= 0)
    age_shift = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    params->buckets[i].bucket_idx = i;
    for (int j = 0; j < HIT_PROB_MAX_AGE; j++) {
      /* initialize to a small number, when the hit density is not available
       * before eviction, we use size to make eviction decision */
      params->buckets[i].hit_prob.hit_density[j] = 1e-8;
      params->buckets[i].hit_prob.age_shift = age_shift;
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
  params->size_bucket_base = LLSC_init_params->size_bucket_base;
  if (params->size_bucket_base <= 0)
    params->size_bucket_base = 1;

  if (params->rank_intvl <= 0) {
    params->rank_intvl = DEFAULT_RANK_INTVL;
  }

  switch (params->type) {
  case SEGCACHE:
  case SEGCACHE_SEG_ORACLE:
  case LOGCACHE_LOG_ORACLE:
  case LOGCACHE_LEARNED:
//      params->obj_score_type = OBJ_SCORE_FREQ_AGE;
      params->obj_score_type = OBJ_SCORE_FREQ_BYTE;
//    params->obj_score_type = OBJ_SCORE_HIT_DENSITY;
    break;
  case SEGCACHE_ITEM_ORACLE:
  case SEGCACHE_BOTH_ORACLE:
  case LOGCACHE_ITEM_ORACLE:
  case LOGCACHE_BOTH_ORACLE: params->obj_score_type = OBJ_SCORE_ORACLE;
    break;
  default: abort();
  };

  params->cache_state.cold_miss_ratio = -1;
  params->cache_state.write_ratio = -1;
  params->cache_state.req_rate = -1;
  params->cache_state.write_rate = -1;

  init_seg_sel(params);
  init_learner(cache, params, LLSC_init_params);
  init_buckets(params, LLSC_init_params->hit_density_age_shift);

  if (params->segs_to_evict == NULL) {
    params->segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
    memset(params->segs_to_evict, 0, sizeof(segment_t *) * params->n_merge);
  }

  cache->hashtable->external_obj = true;
  cache->cache_params = params;
  cache->init_params = init_params;

  INFO(
      "log-structured cache, size %.2lf MB, type %d %s, object selection %d %s, bucket type %d %s, "
      "size_bucket_base %d, n_start_train_seg min %d max %d growth %d sample %d, rank intvl %d, "
      "training truth %d re-train %d\n",
      (double) cache->cache_size / 1048576,
      params->type,
      LSC_type_names[params->type],
      params->obj_score_type,
      obj_score_type_names[params->obj_score_type],
      params->bucket_type,
      bucket_type_names[params->bucket_type],
      params->size_bucket_base,
      0,0,0,
      params->learner.sample_every_n_seg_for_training,
      params->rank_intvl,
      TRAINING_TRUTH,
      params->learner.re_train_intvl
  );
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

#if defined(TRAINING_TRUTH) && TRAINING_TRUTH == TRAINING_TRUTH_ONLINE
__attribute__((unused)) cache_ck_res_e LLSC_check(cache_t *cache,
                                                  request_t *req,
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
    DEBUG_ASSERT(
        ((segment_t *) (cache_obj->LSC.segment))->is_training_seg == false);

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
#if TRAINING_CONSIDER_RETAIN == 1
      if (seg->n_skipped_penalty ++ > params->n_retain_from_seg)
#endif
      {
        double age_since_ev = (double) params->curr_vtime - seg->eviction_vtime;
        if (params->obj_score_type == OBJ_SCORE_FREQ
            || params->obj_score_type == OBJ_SCORE_FREQ_AGE) {
          seg->penalty += 1.0e8 / age_since_ev;
        } else {
          seg->penalty += 1.0e8 / age_since_ev / cache_obj->obj_size;
        }
      }
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
#if TRAINING_CONSIDER_RETAIN == 1
      if (seg->n_skipped_penalty ++ > params->n_retain_from_seg)
#endif
      {
      double age_since_ev = (double) (params->curr_vtime - seg->eviction_vtime);
      if (params->obj_score_type == OBJ_SCORE_FREQ
          || params->obj_score_type == OBJ_SCORE_FREQ_AGE)
        seg->penalty += 1.0e8 / age_since_ev;
      else
        seg->penalty += 1.0e8 / age_since_ev / cache_obj->obj_size;
      }

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
//#elif defined(TRAINING_TRUTH) && TRAINING_TRUTH == TRAINING_TRUTH_ORACLE
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

//  DEBUG_ASSERT(cache_obj->LSC.in_cache);
//  DEBUG_ASSERT(((segment_t *) (cache_obj->LSC.segment))->is_training_seg == false);

  if (cache_obj->LSC.in_cache) {
    seg_hit(params, cache_obj);
    object_hit(params, cache_obj, req);

    return cache_ck_hit;
  } else {

    return cache_ck_miss;
  }
}
#endif

__attribute__((unused)) cache_ck_res_e LLSC_get(cache_t *cache,
                                                request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  if (params->start_rtime == 0)
    params->start_rtime = req->real_time;
  params->curr_rtime = req->real_time;
  params->curr_vtime++;

  cache_ck_res_e ret = cache_get(cache, req);

//  params->cache_state.n_req += 1;
  if (ret == cache_ck_miss)
    params->cache_state.n_miss += 1;

  return ret;
}

__attribute__((unused)) void LLSC_insert(cache_t *cache, request_t *req) {
  LLSC_params_t *params = cache->cache_params;
  bucket_t *bucket = &params->buckets[find_bucket_idx(params, req)];
  segment_t *seg = bucket->last_seg;

  update_cache_state(cache);

  if (seg == NULL || seg->n_total_obj == params->segment_size) {
    DEBUG_ASSERT(seg == NULL || seg->next_seg == NULL);
    if (seg != NULL) {
      seg->req_rate = params->cache_state.req_rate;
      seg->write_rate = params->cache_state.write_rate;
      seg->write_ratio = params->cache_state.write_ratio;
      seg->cold_miss_ratio = params->cache_state.cold_miss_ratio;
    }

    seg = allocate_new_seg(cache, bucket->bucket_idx);
    append_seg_to_bucket(params, bucket, seg);
  }

  cache_obj_t *cache_obj = &seg->objs[seg->n_total_obj];
  copy_request_to_cache_obj(cache_obj, req);
  cache_obj->LSC.LLSC_freq = 0;
  cache_obj->LSC.last_access_rtime = req->real_time;
  //  cache_obj->LSC.last_history_idx = -1;
  cache_obj->LSC.in_cache = 1;
  cache_obj->LSC.segment = seg;
  cache_obj->LSC.idx_in_segment = seg->n_total_obj;
  cache_obj->LSC.next_access_ts = req->next_access_ts;

  hashtable_insert_obj(cache->hashtable, cache_obj);

  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  seg->total_bytes += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
  seg->n_total_obj += 1;

  DEBUG_ASSERT(cache->n_obj > (params->n_segs - params->n_used_buckets)
      * params->segment_size);
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


static bucket_t *select_segs_segcache(cache_t *cache, segment_t **segs) {
  LLSC_params_t *params = cache->cache_params;

  if (params->curr_evict_bucket_idx == -1)
    params->curr_evict_bucket_idx = 0;
  bucket_t *bucket = &params->buckets[params->curr_evict_bucket_idx];
  segment_t *seg_to_evict = params->next_evict_segment;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
    params->curr_evict_bucket_idx =
        (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
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


static bucket_t *select_segs_logUnlearned(cache_t *cache, segment_t **segs) {
  LLSC_params_t *params = cache->cache_params;

  bucket_t *bucket = NULL;
  segment_t *seg_to_evict = NULL;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
    if (bucket != NULL)
      bucket->next_seg_to_evict = bucket->first_seg;
    params->curr_evict_bucket_idx =
        (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];
    seg_to_evict = bucket->next_seg_to_evict;
    n_checked_seg += 1;
    DEBUG_ASSERT(n_checked_seg <= params->n_segs * 2);
  }


  for (int i = 0; i < params->n_merge; i++) {
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }
  bucket->next_seg_to_evict = seg_to_evict;

  return bucket;
}

static inline int find_next_qualified_seg(segment_t **ranked_segs,
                                          int start_pos, int end_pos,
                                          int bucket_idx) {
  for (int i = start_pos; i < end_pos; i++) {
    if (ranked_segs[i] != NULL) {
      if (bucket_idx == -1 || ranked_segs[i]->bucket_idx == bucket_idx) {
        return i;
      }
    }
  }
  return -1;
}

static bucket_t *select_segs_rand(cache_t *cache, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;

  bucket_t *bucket = NULL;
  segment_t *seg_to_evict = NULL;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
//    printf("%d\n", params->curr_evict_bucket_idx);
    bucket = next_unempty_bucket(cache, params->curr_evict_bucket_idx);
    params->curr_evict_bucket_idx = bucket->bucket_idx;
    int n_th = rand() % (bucket->n_seg - params->n_merge + 1);
    seg_to_evict = bucket->first_seg;
    for (int i = 0; i < n_th; i++)
      seg_to_evict = seg_to_evict->next_seg;

    n_checked_seg += 1;
    DEBUG_ASSERT(n_checked_seg <= params->n_segs * 2);
  }

  for (int i = 0; i < params->n_merge; i++) {
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }
  bucket->next_seg_to_evict = seg_to_evict;

  return bucket;


  return bucket;
}


static bucket_t *select_segs(cache_t *cache, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;
  bool array_resized = false;

  if (params->type == SEGCACHE || params->type == SEGCACHE_ITEM_ORACLE
      || params->type == LOGCACHE_ITEM_ORACLE)
    return select_segs_segcache(cache, segs);

  if (params->type == LOGCACHE_LEARNED && params->learner.n_train == 0)
//    return select_segs_segcache(cache, segs);
    return select_segs_logUnlearned(cache, segs);
//    return select_segs_rand(cache, segs);

  /* setup function */
  if (params->seg_sel.ranked_seg_size < params->n_segs) {
    if (params->seg_sel.ranked_segs != NULL) {
      my_free(sizeof(segment_t *) * ranked_seg_size,
              params->seg_sel.ranked_segs);
    }
    params->seg_sel.ranked_segs = my_malloc_n(segment_t *, params->n_segs * 2);
    params->seg_sel.ranked_seg_size = params->n_segs * 2;
    array_resized = true;
  }

  segment_t **ranked_segs = params->seg_sel.ranked_segs;
  int32_t *ranked_seg_pos = &(params->seg_sel.ranked_seg_pos);

  if (params->n_evictions - params->seg_sel.last_rank_time > params->rank_intvl
      || array_resized) {
    rank_segs(cache);
  }

  if (*ranked_seg_pos > params->n_segs / 4) {
    params->rank_intvl /= 2 + 1;
    WARNING("cache size %lu: rank frequency too low, "
            "curr pos in ranked seg %d, total %ld segs, reduce rank_intvl to %d\n",
            (unsigned long) cache->cache_size,
            *ranked_seg_pos,
            (long) params->n_segs,
            params->rank_intvl);
    print_bucket(cache);
    params->seg_sel.last_rank_time = 0;
    return select_segs(cache, segs);
  }

//  if (params->type > LOGCACHE_START_POS) {
//    assert(params->n_merge == 2);
//    int i = 0, j = 0;
//    segs[i++] = ranked_segs[*ranked_seg_pos];
//    ranked_segs[(*ranked_seg_pos)++] = NULL;
//    while (ranked_segs[*ranked_seg_pos] == NULL) {
//      (*ranked_seg_pos)++;
//    }
//
//    while (i < params->n_merge && *ranked_seg_pos + j < params->n_segs / 2) {
//      int bucket_idx = segs[0]->bucket_idx;
//      while (ranked_segs[*ranked_seg_pos + j] == NULL
//             || ranked_segs[*ranked_seg_pos + j]->bucket_idx != bucket_idx) {
//        j += 1;
//        if (*ranked_seg_pos + j >= params->n_segs / 2) {
//          break;
//        }
//      }
//
//      if (ranked_segs[*ranked_seg_pos + j] != NULL
//          && ranked_segs[*ranked_seg_pos + j]->bucket_idx == bucket_idx) {
//        /* we find two segments from the same bucket */
//        segs[i++] = ranked_segs[*ranked_seg_pos + j];
//        ranked_segs[*ranked_seg_pos + j] = NULL;
//        (*ranked_seg_pos)++;
//        while (ranked_segs[*ranked_seg_pos] == NULL) {
//          (*ranked_seg_pos)++;
//        }
//        break;
//      }
//
//      /* we have reached the end of the seg */
//      DEBUG_ASSERT(*ranked_seg_pos + j >= params->n_segs / 2);
//
//      /* we cannot find one more segment with the same bucket id,
//         * we should try the next segment from ranked_seg_pos */
//      i = 0;
//      j = 0;
//      segs[i++] = ranked_segs[*ranked_seg_pos];
//      ranked_segs[(*ranked_seg_pos)++] = NULL;
//      while (ranked_segs[*ranked_seg_pos] == NULL) {
//        (*ranked_seg_pos)++;
//      }
//    }
//    if (*ranked_seg_pos >= params->n_segs / 2) {
//      printf("cache size %ld, %d segs, hard find a matching seg, "
//             "please increase cache size or reduce segment size\n",
//             (long) cache->cache_size, params->n_segs);
//      print_bucket(cache);
//      printf("current ranked pos %d\n", *ranked_seg_pos);
//      for (int m = 0; m < params->n_segs; m++) {
//        if (ranked_segs[m]) printf("seg %d penalty %lf\n", m, ranked_segs[m]->penalty);
//        else
//          printf("seg %d NULL\n", m);
//      }
//      //      abort();
//      params->seg_sel.last_rank_time = 0;
//      //      return select_segs(cache, segs);
//    }
//
//    DEBUG_ASSERT(i == params->n_merge);
//
//    return &params->buckets[segs[0]->bucket_idx];




  if (params->type > LOGCACHE_START_POS) {
    int i, j;
    start:
    i = 0;
    j = find_next_qualified_seg(ranked_segs, *ranked_seg_pos, params->n_segs / 2 + 1, -1);
    segs[i++] = ranked_segs[j];
    ranked_segs[j] = NULL;
    *ranked_seg_pos = j + 1;

    if (j < params->n_segs / 2) {
      while (i < params->n_merge) {
        j = find_next_qualified_seg(ranked_segs,
                                    j + 1,
                                    params->n_segs / 2 + 1,
                                    segs[0]->bucket_idx);
        if (j == -1) {
          goto start;
        }
        segs[i++] = ranked_segs[j];
        ranked_segs[j] = NULL;
      }
      DEBUG_ASSERT(i == params->n_merge);
      DEBUG_ASSERT(segs[0]->bucket_idx == segs[1]->bucket_idx);
      DEBUG_ASSERT(segs[0]->next_seg != NULL);
      DEBUG_ASSERT(segs[1]->next_seg != NULL);

      return &params->buckets[segs[0]->bucket_idx];
    } else {
      printf(
          "cache size %ld, %d segs, current ranked pos %d, hard find a matching seg, "
          "please increase cache size or reduce segment size\n",
          (long) cache->cache_size,
          params->n_segs,
          *ranked_seg_pos);
      print_bucket(cache);
      for (int m = 0; m < params->n_segs; m++) {
        if (ranked_segs[m])
          printf("seg %d penalty %lf\n", m, ranked_segs[m]->penalty);
        else
          printf("seg %d NULL\n", m);
      }
      params->seg_sel.last_rank_time = 0;
      return select_segs(cache, segs);
    }
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

  for (int i = 0; i < params->n_merge; i++) {
    params->learner.n_evicted_bytes += params->segs_to_evict[i]->total_bytes;
  }
  LLSC_merge_segs(cache, bucket, params->segs_to_evict);

  params->n_evictions += 1;

  learner_t *l = &params->learner;

  if (params->type == LOGCACHE_LEARNED) {
    if (params->n_training_segs >= params->learner.n_segs_to_start_training) {
      train(cache);
#ifdef TRAIN_ONCE
      printf("only train once ???\n");
      l->re_train_intvl += l->re_train_intvl * 2000;
#endif

//      learner->next_n_train_seg = learner->min_start_train_seg +
//          learner->n_train_seg_growth * learner->n_train;
//      if (learner->next_n_train_seg > learner->max_start_train_seg)
//        learner->next_n_train_seg = learner->max_start_train_seg;
    } else {
      if (l->last_train_rtime == 0)
        l->last_train_rtime = params->curr_rtime;

      if (params->n_training_segs > l->n_segs_to_start_training / 10) {
        /* we don't want to rate limit at beginning */
        int64_t time_since_last, time_till_next, n_tseg_gap;
        time_since_last = params->curr_rtime - l->last_train_rtime;
        time_till_next = l->re_train_intvl - time_since_last;
        n_tseg_gap = l->n_segs_to_start_training - params->n_training_segs;
        l->sample_every_n_seg_for_training =
            MAX((int) (time_till_next / MAX(n_tseg_gap, 1)), 1);
      }
    }
  }

  if (params->curr_vtime - params->last_hit_prob_compute_vtime
      > HIT_PROB_COMPUTE_INTVL) {
    /* update hit prob for all buckets */
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      update_hit_prob_cdf(&params->buckets[i]);
    }
    params->last_hit_prob_compute_vtime = params->curr_vtime;

//        static int last_print = 0;
//        if (params->curr_rtime - last_print >= 3600 * 24) {
//          printf("cache size %lu ", (unsigned long) cache->cache_size);
//          print_bucket(cache);
//          last_print = params->curr_rtime;
//        }
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
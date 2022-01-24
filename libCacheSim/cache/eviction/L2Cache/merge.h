#pragma once

#include "log.h"
#include "learned.h"
#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"


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

  if (seg1->penalty < seg2->penalty)
    return -1;
  else
    return 1;
}

static void rank_segs(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t **ranked_segs = params->seg_sel.ranked_segs;
  int32_t *ranked_seg_pos = &(params->seg_sel.ranked_seg_pos);

  if (params->type == LOGCACHE_LEARNED) {
    inference(cache);
  }

  int n_seg = 0;
  segment_t *curr_seg;
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    curr_seg = params->buckets[i].first_seg;
    while (curr_seg) {
      if (!is_seg_evictable_fifo(curr_seg, 1) ||
          params->buckets[curr_seg->bucket_idx].n_seg < params->n_merge + 1) {
        curr_seg->penalty = INT32_MAX;
      } else {
        if (params->type == LOGCACHE_LEARNED) {
//            curr_seg->penalty = 0;
        } else if (params->type == LOGCACHE_LOG_ORACLE
            || params->type == SEGCACHE_SEG_ORACLE) {
//            curr_seg->penalty = cal_seg_penalty(cache, params->obj_score_type, curr_seg,
//                                                params->n_retain_from_seg, params->curr_rtime,
//                                                params->curr_vtime);
          curr_seg->penalty = cal_seg_penalty(cache,
                                              OBJ_SCORE_ORACLE,
                                              curr_seg,
                                              params->n_retain_from_seg,
                                              params->curr_rtime,
                                              params->curr_vtime);
        } else if (params->type == SEGCACHE_BOTH_ORACLE
            || params->type == LOGCACHE_BOTH_ORACLE) {
          curr_seg->penalty =
              cal_seg_penalty(cache,
                              OBJ_SCORE_ORACLE,
                              curr_seg,
                              params->n_retain_from_seg,
                              params->curr_rtime,
                              params->curr_vtime);
        } else {
          abort();
        }
      }
      ranked_segs[n_seg++] = curr_seg;
      curr_seg = curr_seg->next_seg;
    }
  }
  DEBUG_ASSERT(n_seg == params->n_segs);

  if (params->type == SEGCACHE_SEG_ORACLE
      || params->type == SEGCACHE_BOTH_ORACLE) {
    qsort(ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_seg);
  } else if (params->type == LOGCACHE_LOG_ORACLE
      || params->type == LOGCACHE_BOTH_ORACLE
      || params->type == LOGCACHE_LEARNED) {
    qsort(ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_log);
  } else {
    abort();
  }

#ifdef dump_ranked_seg_frac
  printf("curr time %ld %ld: ranked segs ", (long) params->curr_rtime, (long) params->curr_vtime);
    for (int d = 0; d < params->n_segs * dump_ranked_seg_frac; d++) {
      printf("%d,", ranked_segs[d]->seg_id);
    }
    printf("\n");
#endif

//    if (params->learner.n_train > 0) {
//      for (int i = 0; i < 20; i++) {
//        print_seg(cache, ranked_segs[i], DEBUG_LEVEL);
//      }
//      printf("\n\n");
//      for (int i = 2000; i < 2020; i++) {
//        print_seg(cache, ranked_segs[i], DEBUG_LEVEL);
//      }
//      printf("\n\n");
//      for (int i = 8000; i < 8020; i++) {
//        print_seg(cache, ranked_segs[i], DEBUG_LEVEL);
//      }
//      printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
//    }

  params->seg_sel.last_rank_time = params->n_evictions;
  *ranked_seg_pos = 0;
//    printf("re-rank\n");

#ifdef ADAPTIVE_RANK
  if (params->rank_intvl * params->n_merge < params->n_segs / 100) {
    /* a better way to balance the rank_intvl */
    if (params->rank_intvl % 100 == 0) {
      DEBUG("cache size %lu increase rank interval from %d to %d\n",
            (unsigned long) cache->cache_size,
            params->rank_intvl,
            params->rank_intvl + 1);
    }
    params->rank_intvl += 1;
  }
#endif
}


void L2Cache_merge_segs(cache_t *cache, bucket_t *bucket, segment_t *segs[]) {
  L2Cache_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(bucket->bucket_idx == segs[0]->bucket_idx);
  DEBUG_ASSERT(bucket->bucket_idx == segs[1]->bucket_idx);

  segment_t *new_seg = allocate_new_seg(cache, bucket->bucket_idx);
//  new_seg->create_rtime = segs[0]->create_rtime;
//  new_seg->create_vtime = segs[0]->create_vtime;

  new_seg->create_rtime = params->curr_rtime;
  new_seg->create_vtime = params->curr_vtime;


  double req_rate = 0, write_rate = 0;
  double write_ratio = 0, cold_miss_ratio = 0;
  int n_merge = 0;
  for (int i = 0; i < params->n_merge; i++) {
    req_rate += segs[i]->req_rate;
    write_rate += segs[i]->write_rate;
    write_ratio += segs[i]->write_ratio;
    cold_miss_ratio += segs[i]->cold_miss_ratio;
    n_merge = MAX(n_merge, segs[i]->n_merge);
  }
  new_seg->req_rate = req_rate / params->n_merge;
  new_seg->write_rate = write_rate / params->n_merge;
  new_seg->write_ratio = write_ratio / params->n_merge;
  new_seg->cold_miss_ratio = cold_miss_ratio / params->n_merge;
  new_seg->n_merge = n_merge + 1;

  link_new_seg_before_seg(params, bucket, segs[0], new_seg);

  cache_obj_t *cache_obj;
  double cutoff =
      find_cutoff(cache,
                  params->obj_score_type,
                  segs,
                  params->n_merge,
                  params->segment_size);

  int n_reuse = 0;
  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(segs[i]->magic == MAGIC);
    for (int j = 0; j < segs[i]->n_total_obj; j++) {
      cache_obj = &segs[i]->objs[j];
      if (cache_obj->L2Cache.next_access_vtime > 0)
        n_reuse += 1;
      if (new_seg->n_total_obj < params->segment_size
          && cal_object_score(params,
                              params->obj_score_type,
                              cache_obj,
                              params->curr_rtime,
                              params->curr_vtime)
              >= cutoff) {
        cache_obj_t *new_obj = &new_seg->objs[new_seg->n_total_obj];
        memcpy(new_obj, cache_obj, sizeof(cache_obj_t));
        new_obj->L2Cache.L2Cache_freq = (new_obj->L2Cache.L2Cache_freq + 1) / 2;
        new_obj->L2Cache.idx_in_segment = new_seg->n_total_obj;
        new_obj->L2Cache.segment = new_seg;
        new_obj->L2Cache.active = 0;
        hashtable_insert_obj(cache->hashtable, new_obj);

        new_seg->n_total_obj += 1;
        new_seg->total_bytes += cache_obj->obj_size;

//        hashtable_delete(cache->hashtable, cache_obj);
      } else {
        cache->n_obj -= 1;
        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
      }
      object_evict(cache, cache_obj);
      cache_obj->L2Cache.in_cache = 0;
    }

#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_EVICTION
    if (params->type == LOGCACHE_LEARNED &&
        params->learner.n_evicted_bytes >
            params->learner.n_bytes_start_collect_train &&
        rand() % params->learner.sample_every_n_seg_for_training == 0) {
      transform_seg_to_training(cache, bucket, segs[i]);
    } else {
      remove_seg_from_bucket(params, bucket, segs[i]);
      clean_one_seg(cache, segs[i]);
    }
#else
//    if (params->curr_rtime - segs[i]->become_train_seg_rtime < )
    remove_seg_from_bucket(params, bucket, segs[i]);
    clean_one_seg(cache, segs[i]);
#endif
  }

  /* not sure if using oracle info */
  if (new_seg->n_total_obj < params->segment_size) {
    WARNING("cutoff %lf %d objects copied %d reuse\n",
            cutoff,
            new_seg->n_total_obj,
            n_reuse);
    abort();
  }
}


static bucket_t *select_segs_segcache(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

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
  L2Cache_params_t *params = cache->eviction_params;

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
  L2Cache_params_t *params = cache->eviction_params;

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
  L2Cache_params_t *params = cache->eviction_params;
  bool array_resized = false;

  if (params->type == SEGCACHE || params->type == SEGCACHE_ITEM_ORACLE
      || params->type == LOGCACHE_ITEM_ORACLE)
    return select_segs_segcache(cache, segs);

  if (params->type == LOGCACHE_LEARNED && params->learner.n_train == 0)
//  if (params->type == LOGCACHE_LEARNED)
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

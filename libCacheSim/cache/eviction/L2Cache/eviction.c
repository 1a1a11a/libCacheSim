
#include "eviction.h"
#include "bucket.h"
#include "learned.h"
#include "segSel.h"
#include "segment.h"
#include "../../dataStructure/hashtable/hashtable.h"

static inline int find_next_qualified_seg(segment_t **ranked_segs, int start_pos, int end_pos,
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

/* choose which segment to evict */
bucket_t *select_segs_to_evict(cache_t *cache, segment_t *segs[]) {
  L2Cache_params_t *params = cache->eviction_params;
  bool array_resized = false;

  if (params->type == SEGCACHE || params->type == LOGCACHE_ITEM_ORACLE)
    return select_segs_fifo(cache, segs);

  if (params->type == LOGCACHE_LEARNED && params->learner.n_train == 0)
    //  if (params->type == LOGCACHE_LEARNED)
    //    return select_segs_fifo(cache, segs);
    return select_segs_logUnlearned(cache, segs);
  //    return select_segs_rand(cache, segs);

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
    rank_segs(cache);
  }

  if (*ranked_seg_pos > params->n_segs / 4) {
    params->rank_intvl /= 2 + 1;
    WARN("cache size %lu: rank frequency too low, "
         "curr pos in ranked seg %d, total %ld segs, reduce rank_intvl to %d\n",
         (unsigned long) cache->cache_size, *ranked_seg_pos, (long) params->n_segs,
         params->rank_intvl);
    // print_bucket(cache);
    params->seg_sel.last_rank_time = 0;
    return select_segs_to_evict(cache, segs);
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
  //      //      return select_segs_to_evict(cache, segs);
  //    }
  //
  //    DEBUG_ASSERT(i == params->n_merge);
  //
  //    return &params->buckets[segs[0]->bucket_idx];

  if (params->type != SEGCACHE) {
    int i, j;
  start:
    i = 0;
    j = find_next_qualified_seg(ranked_segs, *ranked_seg_pos, params->n_segs / 2 + 1, -1);
    segs[i++] = ranked_segs[j];
    ranked_segs[j] = NULL;
    *ranked_seg_pos = j + 1;

    if (j < params->n_segs / 2) {
      while (i < params->n_merge) {
        j = find_next_qualified_seg(ranked_segs, j + 1, params->n_segs / 2 + 1,
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
      printf("cache size %ld, %d segs, current ranked pos %d, hard find a matching seg, "
             "please increase cache size or reduce segment size\n",
             (long) cache->cache_size, params->n_segs, *ranked_seg_pos);
      print_bucket(cache);
      for (int m = 0; m < params->n_segs; m++) {
        if (ranked_segs[m]) printf("seg %d penalty %lf\n", m, ranked_segs[m]->penalty);
        else
          printf("seg %d NULL\n", m);
      }
      params->seg_sel.last_rank_time = 0;
      return select_segs_to_evict(cache, segs);
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
  double miss_ratio = 0;
  int n_merge = 0;
  for (int i = 0; i < params->n_merge; i++) {
    req_rate += segs[i]->req_rate;
    write_rate += segs[i]->write_rate;
    miss_ratio += segs[i]->miss_ratio;
    n_merge = MAX(n_merge, segs[i]->n_merge);
  }
  new_seg->req_rate = req_rate / params->n_merge;
  new_seg->write_rate = write_rate / params->n_merge;
  new_seg->miss_ratio = miss_ratio / params->n_merge;
  new_seg->n_merge = n_merge + 1;

  link_new_seg_before_seg(params, bucket, segs[0], new_seg);

  cache_obj_t *cache_obj;
  double cutoff =
      find_cutoff(cache, params->obj_score_type, segs, params->n_merge, params->segment_size);

  int n_reuse = 0;
  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(segs[i]->magic == MAGIC);
    for (int j = 0; j < segs[i]->n_obj; j++) {
      cache_obj = &segs[i]->objs[j];
      if (cache_obj->L2Cache.next_access_vtime > 0) n_reuse += 1;
      if (new_seg->n_obj < params->segment_size
          && cal_object_score(params, params->obj_score_type, cache_obj, params->curr_rtime,
                              params->curr_vtime)
                 >= cutoff) {
        cache_obj_t *new_obj = &new_seg->objs[new_seg->n_obj];
        memcpy(new_obj, cache_obj, sizeof(cache_obj_t));
        new_obj->L2Cache.freq = (new_obj->L2Cache.freq + 1) / 2;
        new_obj->L2Cache.idx_in_segment = new_seg->n_obj;
        new_obj->L2Cache.segment = new_seg;
        new_obj->L2Cache.active = 0;
        hashtable_insert_obj(cache->hashtable, new_obj);

        new_seg->n_obj += 1;
        new_seg->n_byte += cache_obj->obj_size;

        //        hashtable_delete(cache->hashtable, cache_obj);
      } else {
        cache->n_obj -= 1;
        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
      }
      object_evict(cache, cache_obj);
      cache_obj->L2Cache.in_cache = 0;
    }

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
    if (params->type == LOGCACHE_LEARNED
        && params->cache_state.n_evicted_bytes > params->learner.n_bytes_start_collect_train
        && rand() % params->learner.sample_every_n_seg_for_training == 0) {
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
  if (new_seg->n_obj < params->segment_size) {
    WARN("cutoff %lf %d objects copied %d reuse\n", cutoff, new_seg->n_obj, n_reuse);
    abort();
  }
}
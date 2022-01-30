

#include "segSel.h"
#include "learned.h"
#include "segment.h"
#include "utils.h"

static inline int cmp_seg_log(const void *p1, const void *p2) {
  segment_t *seg1 = *(segment_t **) p1;
  segment_t *seg2 = *(segment_t **) p2;

  DEBUG_ASSERT(seg1->magic == MAGIC);

  if (seg1->penalty < seg2->penalty) return -1;
  else
    return 1;
}

void rank_segs(cache_t *cache) {
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
      if (!is_seg_evictable_fifo(curr_seg, 1)
          || params->buckets[curr_seg->bucket_idx].n_seg < params->n_merge + 1) {
        curr_seg->penalty = INT32_MAX;
      } else {
        if (params->type == LOGCACHE_LEARNED) {
          //            curr_seg->penalty = 0;
        } else if (params->type == LOGCACHE_LOG_ORACLE) {
          //            curr_seg->penalty = cal_seg_penalty(cache, params->obj_score_type, curr_seg,
          //                                                params->n_retain_per_seg, params->curr_rtime,
          //                                                params->curr_vtime);
          curr_seg->penalty =
              cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, params->n_retain_per_seg,
                              params->curr_rtime, params->curr_vtime);
        } else if (params->type == LOGCACHE_BOTH_ORACLE) {
          curr_seg->penalty =
              cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, params->n_retain_per_seg,
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

  if (params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE
      || params->type == LOGCACHE_LEARNED) {
    qsort(ranked_segs, n_seg, sizeof(segment_t *), cmp_seg_log);
  } else {
    abort();
  }

#ifdef dump_ranked_seg_frac
  printf("curr time %ld %ld: ranked segs ", (long) params->curr_rtime,
         (long) params->curr_vtime);
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
            (unsigned long) cache->cache_size, params->rank_intvl, params->rank_intvl + 1);
    }
    params->rank_intvl += 1;
  }
#endif
}

bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

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

bucket_t *select_segs_rand(cache_t *cache, segment_t *segs[]) {
  L2Cache_params_t *params = cache->eviction_params;

  bucket_t *bucket = NULL;
  segment_t *seg_to_evict = NULL;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
    //    printf("%d\n", params->curr_evict_bucket_idx);
    bucket = next_nonempty_bucket(cache, params->curr_evict_bucket_idx);
    params->curr_evict_bucket_idx = bucket->bucket_idx;
    int n_th = rand() % (bucket->n_seg - params->n_merge + 1);
    seg_to_evict = bucket->first_seg;
    for (int i = 0; i < n_th; i++) seg_to_evict = seg_to_evict->next_seg;

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

bucket_t *select_segs_logUnlearned(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

  bucket_t *bucket = NULL;
  segment_t *seg_to_evict = NULL;

  int n_checked_seg = 0;
  while (!is_seg_evictable_fifo(seg_to_evict, params->n_merge)) {
    if (bucket != NULL) bucket->next_seg_to_evict = bucket->first_seg;
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
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

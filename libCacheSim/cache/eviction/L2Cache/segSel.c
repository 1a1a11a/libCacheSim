
#include "segSel.h"
#include "learned.h"
#include "segment.h"
#include "utils.h"
#include "../../../utils/include/mymath.h"

static inline int cmp_seg(const void *p1, const void *p2) {
  segment_t *seg1 = *(segment_t **) p1;
  segment_t *seg2 = *(segment_t **) p2;

  DEBUG_ASSERT(seg1->magic == MAGIC);

  if (seg1->utility < seg2->utility) return -1;
  else
    return 1;
}

// #define DEBUG_CODE
#ifdef DEBUG_CODE
static inline void examine_ranked_seg(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t **ranked_segs = params->seg_sel.ranked_segs;

#ifdef dump_ranked_seg_frac
  printf("curr time %ld %ld: ranked segs ", (long) params->curr_rtime,
         (long) params->curr_vtime);
  for (int d = 0; d < params->n_segs * dump_ranked_seg_frac; d++) {
    printf("%d,", ranked_segs[d]->seg_id);
  }
  printf("\n");
#endif
}

static void check_ranked_segs(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t **ranked_segs = params->seg_sel.ranked_segs;

  int n_segs = params->seg_sel.n_ranked_segs;
  for (int i = 0; i < n_segs; i++) {
    for (int j = i + 1; j < n_segs; j++) {
      if (ranked_segs[i] != NULL) {
        DEBUG_ASSERT(ranked_segs[i]->rank == i);
        DEBUG_ASSERT(ranked_segs[i]->magic == MAGIC);
        DEBUG_ASSERT(ranked_segs[i] != ranked_segs[j]);
      }
      DEBUG_ASSERT(ranked_segs[j] == NULL || ranked_segs[j]->rank == j);
    }
  }
}

static void check_ranked_segs_per_bucket(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t **ranked_segs = params->seg_sel.ranked_segs;

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *seg = params->buckets[bi].ranked_seg_head;
    segment_t *last_seg = seg;
    int n_segs = 0;
    while (seg != NULL) {
      n_segs += 1;
      DEBUG_ASSERT(seg->magic == MAGIC);
      DEBUG_ASSERT(seg->bucket_idx == bi);
      last_seg = seg;
      seg = seg->next_ranked_seg;

      DEBUG_ASSERT(n_segs < params->n_segs * 2);
    }
    DEBUG_ASSERT(n_segs <= params->buckets[bi].n_segs);
    DEBUG_ASSERT(last_seg == params->buckets[bi].ranked_seg_tail);
  }
}
#endif

void rank_segs(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t **ranked_segs = params->seg_sel.ranked_segs;
  int32_t *ranked_seg_pos_p = &(params->seg_sel.ranked_seg_pos);

  if (params->type == LOGCACHE_LEARNED) {
    inference(cache);
  }

  int n_segs = 0;
  segment_t *curr_seg;
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    int j = 0; 
    curr_seg = params->buckets[i].first_seg;
    while (curr_seg) {
      if (params->type == LOGCACHE_LEARNED) {
        // curr_seg->utility = 0;
        // printf("bucket %d, %dth seg, %.4lf %.4lf\n", 
        //         i, j, 
        //         curr_seg->utility, cal_seg_penalty(cache, params->obj_score_type, curr_seg, params->n_retain_per_seg,
        //                     params->curr_rtime, params->curr_vtime)); 
      } else if (params->type == LOGCACHE_LOG_ORACLE) {
        curr_seg->utility =
            cal_seg_penalty(cache, params->obj_score_type, curr_seg, params->n_retain_per_seg,
                            params->curr_rtime, params->curr_vtime);
      } else if (params->type == LOGCACHE_BOTH_ORACLE) {
        curr_seg->utility =
            cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, params->n_retain_per_seg,
                            params->curr_rtime, params->curr_vtime);
      } else {
        abort();
      }

      j += 1; 
      if (!is_seg_evictable(curr_seg, 1, true)
          || params->buckets[curr_seg->bucket_idx].n_segs < params->n_merge + 1) {
        curr_seg->utility += INT32_MAX / 2;
      }

      ranked_segs[n_segs++] = curr_seg;
      curr_seg = curr_seg->next_seg;
    }
  }

  DEBUG_ASSERT(n_segs == params->n_segs);

  DEBUG_ASSERT(params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE
               || params->type == LOGCACHE_LEARNED);
  qsort(ranked_segs, n_segs, sizeof(segment_t *), cmp_seg);

  params->seg_sel.last_rank_time = params->n_evictions;
  params->seg_sel.ranked_seg_pos = 0;

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    params->buckets[bi].ranked_seg_head = NULL;
    params->buckets[bi].ranked_seg_tail = NULL;
  }

  for (int i = 0; i < params->n_segs; i++) {
    ranked_segs[i]->rank = i;

    bucket_t *bkt = &(params->buckets[ranked_segs[i]->bucket_idx]);
    if (bkt->ranked_seg_head == NULL) {
      bkt->ranked_seg_head = ranked_segs[i];
      bkt->ranked_seg_tail = ranked_segs[i];
    } else {
      bkt->ranked_seg_tail->next_ranked_seg = ranked_segs[i];
      bkt->ranked_seg_tail = ranked_segs[i];
    }
    ranked_segs[i]->next_ranked_seg = NULL;
  }
  params->seg_sel.n_ranked_segs = params->n_segs;
}

/** we need to find segments from the same bucket to merge, 
}
 * this is used when we do not require the merged segments to be consecutive 
 */
static inline int find_next_qualified_seg(segment_t **ranked_segs, int start_pos, int end_pos,
                                          int bucket_idx) {
  for (int i = start_pos; i < end_pos; i++) {
    if (ranked_segs[i] != NULL) {
      if (bucket_idx == -1 || ranked_segs[i]->bucket_idx == bucket_idx) {
        if (ranked_segs[i]->next_seg != NULL) {
          return i;
        }
      }
    }
  }
  return -1;
}

/* choose the next segment to evict, use FIFO to choose bucket, 
 * and FIFO within bucket */
bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

  int n_scanned_bucket = 0;
  bucket_t *bucket = &params->buckets[params->curr_evict_bucket_idx];
  segment_t *seg_to_evict = bucket->next_seg_to_evict;
  if (seg_to_evict == NULL) {
    seg_to_evict = bucket->first_seg;
  }

  while (!is_seg_evictable(seg_to_evict, params->n_merge, true)) {
    bucket->next_seg_to_evict = bucket->first_seg;
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];
    seg_to_evict = bucket->next_seg_to_evict;
    if (seg_to_evict == NULL) {
      seg_to_evict = bucket->first_seg;
    }

    n_scanned_bucket += 1;

    if (n_scanned_bucket > MAX_N_BUCKET + 1) {
      DEBUG("cache size %.2lf MB, no evictable seg found\n", (double) cache->cache_size / (1024.0 * 1024.0));
      abort();
      segs[0] = NULL;
      return NULL;
    }
  }

  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(seg_to_evict->bucket_idx == bucket->bucket_idx);
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }

  // TODO: this is not good enough, we should not merge till the end
  if (bucket->n_segs > params->n_merge + 1) {
    bucket->next_seg_to_evict = seg_to_evict;
  } else {
    bucket->next_seg_to_evict = NULL;
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
  }

  return bucket;
}

/* choose bucket with probability being the size of the bucket, 
 * uses FIFO to choose within bucket */
bucket_t *select_segs_weighted_fifo(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

  segment_t *seg_to_evict = NULL;
  bucket_t *bucket = NULL;

  int n_checked_seg = 0;
  // TODO:

  return bucket;
}

/* select segment randomly, we choose bucket in fifo order, 
 * and randomly within the bucket */
bucket_t *select_segs_rand(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

  bucket_t *bucket = NULL;
  segment_t *seg_to_evict = NULL;

  int n_checked_seg = 0;
  while (!is_seg_evictable(seg_to_evict, params->n_merge, true)) {
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];

    int n_th = next_rand() % (bucket->n_segs - params->n_merge);
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
}

/* choose which segment to evict, segs store the segments to evict */
bucket_t *select_segs_learned(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;
  seg_sel_t *ss = &params->seg_sel;

  bool array_resized = false;

  /* setup function */
  if (unlikely(ss->ranked_seg_size < params->n_segs)) {
    if (ss->ranked_segs != NULL) {
      my_free(sizeof(segment_t *) * ss->ranked_seg_size, ss->ranked_segs);
    }
    ss->ranked_seg_size = params->n_segs * 2;
    ss->ranked_segs = my_malloc_n(segment_t *, params->n_segs * 2);
    array_resized = true;
  }

  segment_t **ranked_segs = ss->ranked_segs;
  int32_t *ranked_seg_pos_p = &(ss->ranked_seg_pos);

  /* rerank every rerank_intvl_evictions evictions */
  int rerank_intvl_evictions = params->n_segs * params->rank_intvl / params->n_merge;
  rerank_intvl_evictions = MAX(rerank_intvl_evictions, 1);
  if (params->n_evictions - ss->last_rank_time >= rerank_intvl_evictions || array_resized) {
    rank_segs(cache);
  }

  /* choosing n_merge segments with the lowest utility, may not be consecutive */
  segment_t *seg_to_evict = ranked_segs[*ranked_seg_pos_p];
  // find the segment with the lowest utility
  while (seg_to_evict == NULL && *ranked_seg_pos_p < ss->ranked_seg_size) {
    *ranked_seg_pos_p = *ranked_seg_pos_p + 1;
    seg_to_evict = ranked_segs[*ranked_seg_pos_p];
  }
  // keep the first seg_to_evict in case we cannot find segments to merge, and we
  // need to evict one segment (without retaining)
  segment_t *seg_to_evict_first = seg_to_evict;
  ranked_segs[*ranked_seg_pos_p] = NULL;

  // find the segment with n_mergeable segments
  while (!is_seg_evictable(seg_to_evict, params->n_merge, params->merge_consecutive_segs)) {
    if (*ranked_seg_pos_p > ss->n_ranked_segs * 0.8) {
      // let's evict one seg rather than merging multiple
      segs[0] = seg_to_evict_first;
      bucket_t *bkt = &params->buckets[segs[0]->bucket_idx];
      // this does not hold, because it is possible that bkt->ranked_seg_head was not evictable earlier due to
      // not enough segments, but now become evictable after new segments are inserted
      // DEBUG_ASSERT(bkt->ranked_seg_head == segs[0]);
      bkt->ranked_seg_head = NULL;
      bkt->ranked_seg_tail = NULL;

      // trigger a rerank process
      ss->last_rank_time = 0;
      return NULL;
    }
    *ranked_seg_pos_p = *ranked_seg_pos_p + 1;
    seg_to_evict = ranked_segs[*ranked_seg_pos_p];
    ranked_segs[*ranked_seg_pos_p] = NULL;
  }
  *ranked_seg_pos_p = (*ranked_seg_pos_p + 1);

  // store the n_merge segments
  segs[0] = seg_to_evict;
  for (int i = 1; i < params->n_merge; i++) {
    // next seg could have been evicted
    if (params->merge_consecutive_segs) {
      segs[i] = segs[i - 1]->next_seg;
    } else {
      segs[i] = segs[i - 1]->next_ranked_seg;
    }

    DEBUG_ASSERT(segs[i]->next_seg != NULL);
    DEBUG_ASSERT(segs[i]->bucket_idx == segs[i - 1]->bucket_idx);

    DEBUG_ASSERT(params->merge_consecutive_segs || segs[i]->utility >= segs[i - 1]->utility);
    ranked_segs[segs[i]->rank] = NULL;
  }

  bucket_t *bkt = &params->buckets[segs[0]->bucket_idx];
  bkt->ranked_seg_head = segs[params->n_merge - 1]->next_ranked_seg;
  if (segs[params->n_merge - 1]->next_ranked_seg == NULL) bkt->ranked_seg_tail = NULL;

  // printf("%d %d ######\n", segs[0]->rank, segs[1]->rank);

  return bkt;
}

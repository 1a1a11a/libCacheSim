/* this file handles the segment selection for L2 cache
   it uses FIFO, weighted FIFO, rand, ranking to choose segments 
 */

#include "segSel.h"
#include "../../../utils/include/mymath.h"
#include "learned.h"
#include "segment.h"
#include "utils.h"

static inline int cmp_seg(const void *p1, const void *p2) {
  segment_t *seg1 = *(segment_t **) p1;
  segment_t *seg2 = *(segment_t **) p2;

  DEBUG_ASSERT(seg1->magic == MAGIC);

  if (seg1->pred_utility < seg2->pred_utility) return -1;
  else
    return 1;
}

// #define DEBUG_CODE
#ifdef DEBUG_CODE
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
      DEBUG_ASSERT(seg->bucket_id == bi);
      last_seg = seg;
      seg = seg->next_ranked_seg;

      DEBUG_ASSERT(n_segs < params->n_segs * 2);
    }
    DEBUG_ASSERT(n_segs <= params->buckets[bi].n_segs);
    DEBUG_ASSERT(last_seg == params->buckets[bi].ranked_seg_tail);
  }
}
#endif// DEBUG_CODE

/* rank the segments according to predicted segment utility, 
 * the ranked segments will be used for merge eviction */
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
        ;
      } else if (params->type == LOGCACHE_LOG_ORACLE) {
        curr_seg->pred_utility =
            // cal_seg_utility(cache, params->obj_score_type, curr_seg, params->curr_rtime, params->curr_vtime);
            cal_seg_utility_oracle(cache, curr_seg, params->curr_rtime, params->curr_vtime);
      } else if (params->type == LOGCACHE_BOTH_ORACLE) {
        curr_seg->pred_utility =
            cal_seg_utility_oracle(cache, curr_seg, params->curr_rtime, params->curr_vtime);
      } else {
        abort();
      }

      j += 1;
      if (!is_seg_evictable(curr_seg, 1, true)
          || params->buckets[curr_seg->bucket_id].n_in_use_segs < params->n_merge + 1) {
        // if the segment is the last segment of a bucket or the bucket does not have enough segments
        curr_seg->pred_utility += INT32_MAX / 2;
      }

      ranked_segs[n_segs++] = curr_seg;
      curr_seg = curr_seg->next_seg;
    }
  }

  DEBUG_ASSERT(n_segs == params->n_in_use_segs);

  DEBUG_ASSERT(params->type == LOGCACHE_LOG_ORACLE || params->type == LOGCACHE_BOTH_ORACLE
               || params->type == LOGCACHE_LEARNED);
  qsort(ranked_segs, n_segs, sizeof(segment_t *), cmp_seg);

  // ranked_seg_pos records the position in the ranked_segs array,
  // segment before this position have been evicted
  params->seg_sel.ranked_seg_pos = 0;

  // link the segments in each bucket into chain so that we can easily find the segment ranked next
  // this is used when we do not merge consecutive segments
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    params->buckets[bi].ranked_seg_head = NULL;
    params->buckets[bi].ranked_seg_tail = NULL;
  }

  for (int i = 0; i < params->n_in_use_segs; i++) {
    ranked_segs[i]->rank = i;

    bucket_t *bkt = &(params->buckets[ranked_segs[i]->bucket_id]);
    if (bkt->ranked_seg_head == NULL) {
      bkt->ranked_seg_head = ranked_segs[i];
      bkt->ranked_seg_tail = ranked_segs[i];
    } else {
      bkt->ranked_seg_tail->next_ranked_seg = ranked_segs[i];
      bkt->ranked_seg_tail = ranked_segs[i];
    }
    ranked_segs[i]->next_ranked_seg = NULL;
  }
  params->seg_sel.n_ranked_segs = params->n_in_use_segs;
}

/* when cache size is small and space is fragmented between buckets, 
 * it is possible that there is no bucket with n_merge + 1 segments, 
 * in which case, we randonly pick one and uses eviction w/o merge */
static bucket_t *select_one_seg_to_evict(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;
  bucket_t *bucket = NULL; 

  // no evictable seg found, random+FIFO select one
  int n_th_seg = next_rand() % params->n_in_use_segs;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    if (params->buckets[bi].n_in_use_segs == 0) continue;

    if (n_th_seg > params->buckets[bi].n_in_use_segs) {
      n_th_seg -= params->buckets[bi].n_in_use_segs;
    } else {
      bucket = &params->buckets[bi];
      break;
    }
  }
  segs[0] = bucket->first_seg;
  segs[1] = NULL;
  bucket->next_seg_to_evict = NULL;
  return NULL;
}

/* choose the next segment to evict, use FIFO to choose bucket, 
 * and FIFO within bucket */
bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs) {
  L2Cache_params_t *params = cache->eviction_params;

  int n_scanned_bucket = 0;
  bucket_t *bucket = &params->buckets[params->curr_evict_bucket_idx];
  segment_t *seg_to_evict = bucket->next_seg_to_evict;
  DEBUG_ASSERT(seg_to_evict == NULL || seg_to_evict->bucket_id == bucket->bucket_id);

  while (!is_seg_evictable(seg_to_evict, params->n_merge, true)) {
    bucket->next_seg_to_evict = bucket->first_seg;
    params->curr_evict_bucket_idx = (params->curr_evict_bucket_idx + 1) % MAX_N_BUCKET;
    bucket = &params->buckets[params->curr_evict_bucket_idx];
    seg_to_evict = bucket->next_seg_to_evict;
    if (seg_to_evict == NULL) {
      seg_to_evict = bucket->first_seg;
    }
    DEBUG_ASSERT(seg_to_evict == NULL || seg_to_evict->bucket_id == bucket->bucket_id);

    n_scanned_bucket += 1;

    if (n_scanned_bucket > MAX_N_BUCKET + 1) {
      return select_one_seg_to_evict(cache, segs);
    }
  }

  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(seg_to_evict->bucket_id == bucket->bucket_id);
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }

  if (bucket->n_in_use_segs > params->n_merge + 1) {
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

  int r = next_rand() % params->n_in_use_segs;
  /* select a bucket based on the probability weighted using the bucket size */
  for (int bi = 0; bi < MAX_N_BUCKET * 2; bi++) {
    int bucket_idx = bi % MAX_N_BUCKET; 
    if (params->buckets[bucket_idx].n_in_use_segs < params->n_merge + 1) continue;

    if (r > params->buckets[bucket_idx].n_in_use_segs) {
      r -= params->buckets[bucket_idx].n_in_use_segs;
    } else {
      seg_to_evict = params->buckets[bucket_idx].next_seg_to_evict;
      if (seg_to_evict == NULL) {
        seg_to_evict = params->buckets[bucket_idx].first_seg;
      }
      if (is_seg_evictable(seg_to_evict, params->n_merge, true)) {
        bucket = &params->buckets[bucket_idx];
        break;
      } else {
        params->buckets[bucket_idx].next_seg_to_evict = NULL; 
      }
    }
  }

  if (bucket == NULL) {
      // no evictable seg found, random+FIFO select one
    return select_one_seg_to_evict(cache, segs); 
  }

  for (int i = 0; i < params->n_merge; i++) {
    DEBUG_ASSERT(seg_to_evict->bucket_id == bucket->bucket_id);
    segs[i] = seg_to_evict;
    seg_to_evict = seg_to_evict->next_seg;
  }

  if (bucket->n_in_use_segs > params->n_merge * 2 + 1) {
    bucket->next_seg_to_evict = seg_to_evict;
  } else {
    bucket->next_seg_to_evict = NULL;
  }
  
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

    int n_th = next_rand() % (bucket->n_in_use_segs - params->n_merge);
    seg_to_evict = bucket->first_seg;
    for (int i = 0; i < n_th; i++) seg_to_evict = seg_to_evict->next_seg;

    n_checked_seg += 1;
    DEBUG_ASSERT(n_checked_seg <= params->n_in_use_segs * 2);
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
  if (unlikely(ss->ranked_seg_size < params->n_in_use_segs)) {
    if (ss->ranked_segs != NULL) {
      my_free(sizeof(segment_t *) * ss->ranked_seg_size, ss->ranked_segs);
    }
    ss->ranked_seg_size = params->n_in_use_segs * 2;
    ss->ranked_segs = my_malloc_n(segment_t *, params->n_in_use_segs * 2);
    array_resized = true;
  }

  segment_t **ranked_segs = ss->ranked_segs;
  int32_t *ranked_seg_pos_p = &(ss->ranked_seg_pos);

  if (*ranked_seg_pos_p >= ss->n_ranked_segs * params->rank_intvl || array_resized) {
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
      segs[1] = NULL;
      bucket_t *bkt = &params->buckets[segs[0]->bucket_id];
      // this does not hold, because it is possible that bkt->ranked_seg_head was not evictable earlier due to
      // not enough segments, but now become evictable after new segments are inserted
      // DEBUG_ASSERT(bkt->ranked_seg_head == segs[0]);
      bkt->ranked_seg_head = NULL;
      bkt->ranked_seg_tail = NULL;

      // trigger a rerank process
      ss->ranked_seg_pos = INT32_MAX;
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
    DEBUG_ASSERT(segs[i]->bucket_id == segs[i - 1]->bucket_id);

    DEBUG_ASSERT(params->merge_consecutive_segs
                 || segs[i]->pred_utility >= segs[i - 1]->pred_utility);

    if (segs[i]->rank != -1) ranked_segs[segs[i]->rank] = NULL;
  }

  bucket_t *bkt = &params->buckets[segs[0]->bucket_id];
  bkt->ranked_seg_head = segs[params->n_merge - 1]->next_ranked_seg;
  if (segs[params->n_merge - 1]->next_ranked_seg == NULL) bkt->ranked_seg_tail = NULL;

  // printf("%d %d ######\n", segs[0]->rank, segs[1]->rank);
  // printf("about to clean seg %d %d - rank %d %d - pos %d\n", segs[0]->seg_id, segs[1]->seg_id, segs[0]->rank, segs[1]->rank, *ranked_seg_pos_p);
  DEBUG_ASSERT(segs[0]->rank != -1);

  return bkt;
}

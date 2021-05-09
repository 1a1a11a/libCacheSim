#pragma once

#include "log.h"
#include "learned.h"
#include "../../include/libCacheSim/evictionAlgo/LLSC.h"


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
  LLSC_params_t *params = cache->cache_params;
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
//          curr_seg->penalty = 1.0 / curr_seg->total_bytes;
          ;
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


void LLSC_merge_segs(cache_t *cache, bucket_t *bucket, segment_t *segs[]) {
  LLSC_params_t *params = cache->cache_params;

  DEBUG_ASSERT(bucket->bucket_idx == segs[0]->bucket_idx);
  DEBUG_ASSERT(bucket->bucket_idx == segs[1]->bucket_idx);

  segment_t *new_seg = allocate_new_seg(cache, bucket->bucket_idx);
//  new_seg->create_rtime = segs[0]->create_rtime;
//  new_seg->create_vtime = segs[0]->create_vtime;

  new_seg->create_rtime = params->curr_rtime;
  new_seg->create_vtime = params->curr_vtime;


  double req_rate = 0, write_rate = 0;
  int n_merge = 0;
  for (int i = 0; i < params->n_merge; i++) {
    req_rate += segs[i]->req_rate;
    write_rate += segs[i]->write_rate;
    n_merge = MAX(n_merge, segs[i]->n_merge);
  }
  new_seg->req_rate = req_rate / params->n_merge;
  new_seg->write_rate = write_rate / params->n_merge;
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
      if (cache_obj->LSC.next_access_ts > 0)
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
        new_obj->LSC.LLSC_freq = (new_obj->LSC.LLSC_freq + 1) / 2;
        new_obj->LSC.idx_in_segment = new_seg->n_total_obj;
        new_obj->LSC.segment = new_seg;
        if (new_obj->LSC.n_merged < 1u<<11u)
          new_obj->LSC.n_merged += 1;
        new_obj->LSC.active = 0;
        hashtable_insert_obj(cache->hashtable, new_obj);

        new_seg->n_total_obj += 1;
        new_seg->total_bytes += cache_obj->obj_size;

//        hashtable_delete(cache->hashtable, cache_obj);
      } else {
        cache->n_obj -= 1;
        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
      }
      object_evict(cache, cache_obj);
      cache_obj->LSC.in_cache = 0;
    }

    if (params->type == LOGCACHE_LEARNED &&
        params->learner.n_evicted_bytes >
            params->learner.n_bytes_start_collect_train &&
        rand() % params->learner.sample_every_n_seg_for_training == 0) {
      transform_seg_to_training(cache, bucket, segs[i]);
    } else {
      remove_seg_from_bucket(params, bucket, segs[i]);
      clean_one_seg(cache, segs[i]);
    }
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

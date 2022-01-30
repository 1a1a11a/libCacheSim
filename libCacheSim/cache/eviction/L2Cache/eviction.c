
#include "eviction.h"
#include "../../dataStructure/hashtable/hashtable.h"
#include "bucket.h"
#include "learned.h"
#include "segSel.h"
#include "segment.h"

/* choose which segment to evict */
bucket_t *select_segs_to_evict(cache_t *cache, segment_t *segs[]) {
  L2Cache_params_t *params = cache->eviction_params;

  if (params->type == SEGCACHE || params->type == LOGCACHE_ITEM_ORACLE) {
    return select_segs_fifo(cache, segs);
  }

  if (params->type == LOGCACHE_LEARNED || params->type == LOGCACHE_LOG_ORACLE) {
    if (params->learner.n_train <= 0) {
      return select_segs_fifo(cache, segs);
      //    return select_segs_rand(cache, segs);
    }
    return select_segs_learned(cache, segs);
  }

  if (params->type == LOGCACHE_BOTH_ORACLE) {
    return select_segs_learned(cache, segs);      
  }

  assert(0);// should not reach here
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
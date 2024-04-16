
#include "../../../dataStructure/hashtable/hashtable.h"
#include "GLCacheInternal.h"
#include "const.h"
#include "obj.h"

/* choose which segment to evict */
bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs) {
  GLCache_params_t *params = cache->eviction_params;

  if (params->type == LOGCACHE_ITEM_ORACLE) {
    if (params->learner.n_train <= 0) {
      return select_segs_fifo(cache, segs);
    }
    return select_segs_learned(cache, segs);
  }

  if (params->type == LOGCACHE_LEARNED) {
    if (params->learner.n_train <= 0) {
      return select_segs_fifo(cache, segs);
    }
    return select_segs_learned(cache, segs);
  }

  if (params->type == LOGCACHE_TWO_ORACLE ||
      params->type == LOGCACHE_LOG_ORACLE) {
    return select_segs_learned(cache, segs);
  }

  assert(0);  // should not reach here
}

void transform_seg_to_training(cache_t *cache, bucket_t *bucket,
                               segment_t *seg) {
  GLCache_params_t *params = cache->eviction_params;
  seg->become_train_seg_vtime = params->curr_vtime;
  seg->become_train_seg_rtime = params->curr_rtime;
  seg->train_utility = 0;

  /* remove from the bucket */
  remove_seg_from_bucket(params, bucket, seg);

  append_seg_to_bucket(params, &params->train_bucket, seg);
}

/* make sure the segment is not in the bucket */
static void assert_seg_not_in_bucket(GLCache_params_t *params, bucket_t *bucket,
                                     segment_t *seg_to_find) {
  DEBUG_ASSERT(bucket->bucket_id == seg_to_find->bucket_id);
  segment_t *seg = bucket->first_seg;
  while (seg != NULL) {
    DEBUG_ASSERT(seg != seg_to_find);
    seg = seg->next_seg;
  }
}

/** merge multiple segments into one segment **/
void GLCache_merge_segs(cache_t *cache, bucket_t *bucket, segment_t **segs) {
  GLCache_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(bucket->bucket_id == segs[0]->bucket_id);
  DEBUG_ASSERT(bucket->bucket_id == segs[1]->bucket_id);

  segment_t *new_seg = allocate_new_seg(cache, bucket->bucket_id);
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
  double cutoff = find_cutoff(cache, params->obj_score_type, segs,
                              params->n_merge, params->segment_size);

#ifdef RANDOMIZE_MERGE
  int pos = 0;
#endif
  for (int i = 0; i < params->n_merge; i++) {
    // merge n segments into one segment
    DEBUG_ASSERT(segs[i]->magic == MAGIC);
    for (int j = 0; j < segs[i]->n_obj; j++) {
      cache_obj = &segs[i]->objs[j];
#ifdef RANDOMIZE_MERGE
      double obj_score = params->obj_sel.score_array_offset[pos++];
#else
      double obj_score =
          cal_obj_score(params, params->obj_score_type, cache_obj);
#endif
      if (new_seg->n_obj < params->segment_size && obj_score >= cutoff) {
        cache_obj_t *new_obj = &new_seg->objs[new_seg->n_obj];
        memcpy(new_obj, cache_obj, sizeof(cache_obj_t));
        new_obj->GLCache.freq = (new_obj->GLCache.freq + 1) / 2;
        new_obj->GLCache.idx_in_segment = new_seg->n_obj;
        new_obj->GLCache.segment = new_seg;
        new_obj->GLCache.active = 0;
        new_obj->GLCache.in_cache = 1;
        new_obj->GLCache.seen_after_snapshot = 0;
        hashtable_insert_obj(cache->hashtable, new_obj);

        new_seg->n_obj += 1;
        new_seg->n_byte += cache_obj->obj_size;
      } else {
        cache->n_obj -= 1;
        cache->occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
      }
      obj_evict_update(cache, cache_obj);
      cache_obj->GLCache.in_cache = 0;

      if (cache_obj->GLCache.seen_after_snapshot == 1) {
        /* do not need to keep a ghost in the hashtable */
        hashtable_delete(cache->hashtable, cache_obj);
      }
    }

    if (segs[i]->selected_for_training) {
      transform_seg_to_training(cache, bucket, segs[i]);
    } else {
      remove_seg_from_bucket(params, bucket, segs[i]);
      clean_one_seg(cache, segs[i]);
    }
  }
}

// called when there is no segment can be merged due to fragmentation
// different from clean_one_seg because this function also updates cache state
int evict_one_seg(cache_t *cache, segment_t *seg) {
  VVERBOSE("req %lu, evict one seg id %d occupied size %lu/%lu\n", cache->n_req,
           seg->seg_id, cache->occupied_byte, cache->cache_size);
  GLCache_params_t *params = cache->eviction_params;
  bucket_t *bucket = &params->buckets[seg->bucket_id];

  int n_cleaned = 0;
  for (int i = 0; i < seg->n_obj; i++) {
    cache_obj_t *cache_obj = &seg->objs[i];

    obj_evict_update(cache, cache_obj);

    // if (hashtable_try_delete(cache->hashtable, cache_obj)) {
    if (cache_obj->GLCache.in_cache == 1) {
      cache_obj->GLCache.in_cache = 0;

      n_cleaned += 1;
      cache->n_obj -= 1;
      cache->occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
    }

    if (seg->selected_for_training &&
        cache_obj->GLCache.seen_after_snapshot == 1) {
      /* do not need to keep a ghost in the hashtable */
      hashtable_delete(cache->hashtable, cache_obj);
    }
  }

  if (seg->selected_for_training) {
    transform_seg_to_training(cache, bucket, seg);
  } else {
    remove_seg_from_bucket(params, bucket, seg);
    clean_one_seg(cache, seg);
  }

  return n_cleaned;
}

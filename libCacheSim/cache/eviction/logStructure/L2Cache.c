//
//  a learned log-structure cache module
//
//

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "learned.h"
#include "log.h"
#include "init.h"
#include "oracle.h"
#include "utils.h"
#include "merge.h"


#include <assert.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


  cache_t *L2Cache_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("L2Cache", ccache_params);
  L2Cache_init_params_t *L2Cache_init_params = init_params;

  cache->hashtable->external_obj = true;
  cache->init_params = init_params;

  L2Cache_params_t *params = my_malloc(L2Cache_params_t);
  memset(params, 0, sizeof(L2Cache_params_t));
  cache->eviction_params = params;


  params->curr_evict_bucket_idx = -1;
  params->segment_size = L2Cache_init_params->segment_size;
  params->n_merge = L2Cache_init_params->n_merge;
  params->n_retain_from_seg = params->segment_size / params->n_merge;
  params->rank_intvl = L2Cache_init_params->rank_intvl;
  params->type = L2Cache_init_params->type;
  params->bucket_type = L2Cache_init_params->bucket_type;
  params->size_bucket_base = L2Cache_init_params->size_bucket_base;
  if (params->size_bucket_base <= 0) {
    params->size_bucket_base = 1;
  }

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

  init_seg_sel(cache);
  init_learner(cache, L2Cache_init_params);
  init_buckets(cache, L2Cache_init_params->hit_density_age_shift);
  init_cache_state(cache);

  INFO(
      "log-structured cache, size %.2lf MB, type %d %s, object selection %d %s, bucket type %d %s, "
      "size_bucket_base %d, %ld bytes start training seg collection, sample %d, rank intvl %d, "
      "training truth %d re-train %d\n",
      (double) cache->cache_size / 1048576,
      params->type,
      LSC_type_names[params->type],
      params->obj_score_type,
      obj_score_type_names[params->obj_score_type],
      params->bucket_type,
      bucket_type_names[params->bucket_type],
      params->size_bucket_base,
//      (long) params->learner.n_bytes_start_collect_train,
//      params->learner.sample_every_n_seg_for_training,
      0L, 0,
      params->rank_intvl,
      TRAINING_TRUTH,
      0
//      params->learner.re_train_intvl
  );
  return cache;
}

__attribute__((unused)) void L2Cache_free(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
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

  my_free(sizeof(L2Cache_params_t), params);
  cache_struct_free(cache);
}

#if defined(TRAINING_TRUTH) && TRAINING_TRUTH == TRAINING_TRUTH_ONLINE
__attribute__((unused)) cache_ck_res_e L2Cache_check(cache_t *cache,
                                                  request_t *req,
                                                  bool update_cache) {
  L2Cache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    assert(0);
  }

  if (cache_obj->L2Cache.in_cache) {
    /* if there is an evicted entry, the evicted entry should be after this entry */
    DEBUG_ASSERT(
        ((segment_t *) (cache_obj->L2Cache.segment))->is_training_seg == false);

    /* seg_hit update segment state features */
    seg_hit(params, cache_obj);
    /* object hit update training data y and object stat */
    object_hit(params, cache_obj, req);

#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_CACHE
    update_train_y(params, cache_obj);
#endif

    cache_obj = cache_obj->hash_next;
    while (cache_obj && cache_obj->obj_id != req->obj_id) {
      cache_obj = cache_obj->hash_next;
    }

    while (cache_obj) {
      /* history entry, it can have more than one history entry, when an object
       * is retained multiple times then get a hit,
       * we can delete the history upon the second merge, or
       * we can remove it here */
      update_train_y(params, cache_obj);
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
      update_train_y(params, cache_obj);
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
__attribute__((unused)) cache_ck_res_e L2Cache_check(cache_t *cache, request_t *req,
                                                  bool update_cache) {
  L2Cache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    return cache_ck_hit;
  }

  if (cache_obj->L2Cache.in_cache) {
    seg_hit(params, cache_obj);
    object_hit(params, cache_obj, req);

    return cache_ck_hit;
  } else {

    return cache_ck_miss;
  }
}
#endif

__attribute__((unused)) cache_ck_res_e L2Cache_get(cache_t *cache,
                                                request_t *req) {
  L2Cache_params_t *params = cache->eviction_params;
  if (params->start_rtime == 0)
    params->start_rtime = req->real_time;
  params->curr_rtime = req->real_time;
  params->curr_vtime++;

  cache_ck_res_e ret = cache_get_base(cache, req);

  if (ret == cache_ck_miss)
    params->cache_state.n_miss += 1;


#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_CACHE
  if (params->type == LOGCACHE_LEARNED) {
    learner_t *l = &params->learner;
    if (l->n_evicted_bytes >= cache->cache_size / 2) {
      if (l->n_snapshot == 0) {
        /* move to initialization */
        create_data_holder2(cache);
        l->last_train_rtime = params->curr_rtime;
      }
      if (params->curr_rtime - l->last_train_rtime >= l->re_train_intvl) {
        train(cache);
      }
      if (params->curr_rtime - l->last_snapshot_rtime > l->snapshot_intvl)
        snapshot_segs_to_training_data(cache);
    }
  }
#endif
  return ret;
}

__attribute__((unused)) void L2Cache_insert(cache_t *cache, request_t *req) {
  L2Cache_params_t *params = cache->eviction_params;
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
  cache_obj->L2Cache.L2Cache_freq = 0;
  cache_obj->L2Cache.last_access_rtime = req->real_time;
  //  cache_obj->L2Cache.last_history_idx = -1;
  cache_obj->L2Cache.in_cache = 1;
  cache_obj->L2Cache.segment = seg;
  cache_obj->L2Cache.seen_after_snapshot = 0;
  cache_obj->L2Cache.idx_in_segment = seg->n_total_obj;
  cache_obj->L2Cache.next_access_ts = req->next_access_ts;

  hashtable_insert_obj(cache->hashtable, cache_obj);

  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  seg->total_bytes += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
  seg->n_total_obj += 1;

  DEBUG_ASSERT(cache->n_obj > (params->n_segs - params->n_used_buckets)
      * params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_segs * params->segment_size);
}


void L2Cache_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *l = &params->learner;

  bucket_t *bucket = select_segs(cache, params->seg_sel.segs_to_evict);

  params->n_evictions += 1;
  for (int i = 0; i < params->n_merge; i++) {
    l->n_evicted_bytes += params->seg_sel.segs_to_evict[i]->total_bytes;
  }


  if (params->type == LOGCACHE_LEARNED) {
#if TRAINING_DATA_SOURCE == TRAINING_DATA_FROM_EVICTION
    if (params->n_training_segs >= params->learner.n_segs_to_start_training) {
//    if (params->curr_rtime - l->last_train_rtime >= l->re_train_intvl) {
      train(cache);
#ifdef TRAIN_ONCE
      printf("only train once ???\n");
      l->re_train_intvl += l->re_train_intvl * 2000;
#endif
    } else {
      if (l->last_train_rtime == 0) {
        /* initialize the last_train_rtime */
        l->last_train_rtime = params->curr_rtime;
      }

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
#else

#endif
  }


    L2Cache_merge_segs(cache, bucket, params->seg_sel.segs_to_evict);


  if (params->obj_score_type == OBJ_SCORE_HIT_DENSITY &&
        params->curr_vtime - params->last_hit_prob_compute_vtime > HIT_PROB_COMPUTE_INTVL) {
    /* update hit prob for all buckets */
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      update_hit_prob_cdf(&params->buckets[i]);
    }
    params->last_hit_prob_compute_vtime = params->curr_vtime;
  }
//        static int last_print = 0;
//        if (params->curr_rtime - last_print >= 3600 * 24) {
//          printf("cache size %lu ", (unsigned long) cache->cache_size);
//          print_bucket(cache);
//          last_print = params->curr_rtime;
//        }
}

void L2Cache_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  L2Cache_params_t *params = cache->eviction_params;
  abort();

  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, cache_obj);
  hashtable_delete(cache->hashtable, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;
}

void L2Cache_remove(cache_t *cache, obj_id_t obj_id) {
  abort();

  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;

  hashtable_delete(cache->hashtable, cache_obj);
}

#ifdef __cplusplus
}
#endif

//
//  a learned log-structure cache module
//
//

#include "../../include/libCacheSim/evictionAlgo/GLCache.h"

#include <assert.h>
#include <stdbool.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "GLCacheInternal.h"
#include "bucket.h"
#include "cacheState.h"
#include "const.h"
#include "eviction.h"
#include "learned.h"
#include "obj.h"
#include "segSel.h"
#include "segment.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* output file for comparing online and offline calculated segment utility */
FILE *ofile_cmp_y = NULL;

static void set_default_init_params(GLCache_init_params_t *init_params) {
  init_params->segment_size = 100;
  init_params->n_merge = 2;
  init_params->rank_intvl = 0.05;
  init_params->merge_consecutive_segs = true;
  init_params->retrain_intvl = 86400 * 2;
  init_params->train_source_y = TRAIN_Y_FROM_ONLINE;
  init_params->type = LOGCACHE_LEARNED;
}

const char *GLCache_default_params(void) {
  return "segment_size=100;n_merge=2;"
         "type=LOGCACHE_LEARNED;rank_intvl=0.05;"
         "merge_consecutive_segs=true;train_source_y=TRAIN_Y_FROM_ONLINE;"
         "retrain_intvl=172800;";
}

static void parse_init_params(const char *cache_specific_params,
                       GLCache_init_params_t *params) {
  char *params_str = strdup(cache_specific_params);

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ";");
    if (strcasecmp(key, "segment_size") == 0) {
      params->segment_size = atoi(value);
    } else if (strcasecmp(key, "n_merge") == 0) {
      params->n_merge = atoi(value);
    } else if (strcasecmp(key, "rank_intvl") == 0) {
      params->rank_intvl = atof(value);
    } else if (strcasecmp(key, "merge_consecutive_segs") == 0) {
      params->merge_consecutive_segs = atoi(value);
    } else if (strcasecmp(key, "retrain_intvl") == 0) {
      params->retrain_intvl = atoi(value);
    } else if (strcasecmp(key, "train_source_y") == 0) {
      if (strcasecmp(value, "online") == 0) {
        params->train_source_y = TRAIN_Y_FROM_ONLINE;
      } else if (strcasecmp(value, "oracle") == 0) {
        params->train_source_y = TRAIN_Y_FROM_ORACLE;
      } else {
        ERROR("Unknown train_source_y %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "type") == 0) {
      if (strcasecmp(value, "learned") == 0) {
        params->type = LOGCACHE_LEARNED;
      } else if (strcasecmp(value, "logOracle") == 0) {
        params->type = LOGCACHE_LOG_ORACLE;
      } else if (strcasecmp(value, "itemOracle") == 0) {
        params->type = LOGCACHE_ITEM_ORACLE;
      } else if (strcasecmp(value, "twoOracle") == 0) {
        params->type = LOGCACHE_BOTH_ORACLE;
      } else {
        ERROR("Unknown type %s\n", value);
        exit(1);
      }
    } else {
      ERROR("GLCache does not have parameter %s\n", key);
      exit(1);
    }
  }
}

cache_t *GLCache_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GLCache", ccache_params);

  cache->hashtable->external_obj = true;
  cache->init_params = cache_specific_params;

  GLCache_init_params_t init_params;
  set_default_init_params(&init_params);
  parse_init_params(cache_specific_params, &init_params);
  check_init_params(&init_params);

  GLCache_params_t *params = my_malloc(GLCache_params_t);
  memset(params, 0, sizeof(GLCache_params_t));
  cache->eviction_params = params;

  params->curr_evict_bucket_idx = 0;
  params->start_rtime = -1;
  params->last_hit_prob_compute_vtime = 0;
  params->last_hit_prob_compute_rtime = 0;

  params->type = init_params.type;
  params->train_source_y = init_params.train_source_y;

  params->merge_consecutive_segs = init_params.merge_consecutive_segs;
  params->segment_size = init_params.segment_size;
  params->n_merge = init_params.n_merge;
  params->n_retain_per_seg = params->segment_size / params->n_merge;
  params->rank_intvl = init_params.rank_intvl;

  switch (params->type) {
    case SEGCACHE:
    case LOGCACHE_LOG_ORACLE:
    case LOGCACHE_LEARNED:
      params->obj_score_type = OBJ_SCORE_SIZE_AGE;
      break;
    case LOGCACHE_ITEM_ORACLE:
    case LOGCACHE_BOTH_ORACLE:
      params->obj_score_type = OBJ_SCORE_ORACLE;
      break;
    default:
      abort();
  };

  init_global_params();
  init_seg_sel(cache);
  init_obj_sel(cache);
  init_learner(cache, init_params.retrain_intvl);
  init_cache_state(cache);

  cache->cache_init = GLCache_init;
  cache->cache_free = GLCache_free;
  cache->get = GLCache_get;
  cache->check = GLCache_check;
  cache->insert = GLCache_insert;
  cache->evict = GLCache_evict;
  cache->remove = GLCache_remove;

  INFO(
      "%s, %.0lfMB, training_interval %d, source %d, "
      "rank interval %.2lf, merge consecutive segments %d, "
      "merge %d segments\n",
      GLCache_type_names[params->type], (double)cache->cache_size / 1048576.0,
      // obj_score_type_names[params->obj_score_type],
      // bucket_type_names[params->bucket_type],
      params->learner.retrain_intvl, params->train_source_y, params->rank_intvl,
      params->merge_consecutive_segs, params->n_merge);
  return cache;
}

void GLCache_free(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;
  bucket_t *bkt = &params->train_bucket;
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

  my_free(sizeof(double) * params->obj_sel.array_size,
          params->obj_sel.score_array);
  my_free(sizeof(dd_pair_t) * params->obj_sel.array_size,
          params->obj_sel.dd_pair_array);
  my_free(sizeof(segment_t *) * params->n_merge, params->obj_sel.segs_to_evict);

  my_free(sizeof(segment_t *) * params->n_seg, params->seg_sel.ranked_segs);
  my_free(sizeof(feature_t) * params->learner.train_matrix_n_row,
          params->learner.train_x);
  my_free(sizeof(feature_t) * params->learner.train_matrix_n_row,
          params->learner.train_y);
  my_free(sizeof(feature_t) * params->learner.train_matrix_n_row,
          params->learner.train_y_oracle);
  my_free(sizeof(feature_t) * params->learner.valid_matrix_n_row,
          params->learner.valid_x);
  my_free(sizeof(feature_t) * params->learner.valid_matrix_n_row,
          params->learner.valid_y);
  my_free(sizeof(pred_t) * params->learner.inf_matrix_n_row,
          params->learner.inference_x);

  my_free(sizeof(GLCache_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e GLCache_check(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  GLCache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  if (!update_cache) {
    assert(0);
  }

  int n_in_cache = 0;
  while (cache_obj != NULL) {
    /* a cache obj can be a cached object, or one of the objects on the evicted
     * segments */
    if (cache_obj->obj_id != req->obj_id) {
      cache_obj = cache_obj->hash_next;
      continue;
    }

    segment_t *seg = cache_obj->GLCache.segment;

    if (cache_obj->GLCache.in_cache == 1) {
      // update features
      n_in_cache++;

      /* seg_hit_update update segment state features */
      seg_hit_update(params, cache_obj);
      /* object hit update training data y and object stat */
      obj_hit_update(params, cache_obj, req);

      if (seg->selected_for_training) {
        cache_obj->GLCache.seen_after_snapshot = 1;
        update_train_y(params, cache_obj);
      }

    } else {
      DEBUG_ASSERT(seg->selected_for_training == true);
      DEBUG_ASSERT(cache_obj->GLCache.seen_after_snapshot == 0);

      cache_obj->GLCache.seen_after_snapshot = 1;
      update_train_y(params, cache_obj);

      /* remove object from hash table */
      hashtable_delete(cache->hashtable, cache_obj);
    }

    cache_obj = cache_obj->hash_next;
  }

  DEBUG_ASSERT(n_in_cache <= 1);

  return cache_ck_hit;
}

cache_ck_res_e GLCache_get(cache_t *cache, const request_t *req) {
  GLCache_params_t *params = cache->eviction_params;

  cache_ck_res_e ret = cache_get_base(cache, req);

  if (params->type == LOGCACHE_LEARNED ||
      params->type == LOGCACHE_ITEM_ORACLE) {
    /* generate training data by taking a snapshot */
    learner_t *l = &params->learner;
    if (params->curr_rtime > 86400) {
      if (l->n_train == -1) {
        snapshot_segs_to_training_data(cache);
        l->last_train_rtime = params->curr_rtime;
        l->n_train = 0;
      } else if (params->curr_rtime - l->last_train_rtime >=
                 l->retrain_intvl + 1) {
        train(cache);
        snapshot_segs_to_training_data(cache);
      }
    }
  }

  update_cache_state(cache, req, ret);

  return ret;
}

void GLCache_insert(cache_t *cache, const request_t *req) {
  GLCache_params_t *params = cache->eviction_params;
  bucket_t *bucket = &params->buckets[0];
  segment_t *seg = bucket->last_seg;
  DEBUG_ASSERT(seg == NULL || seg->next_seg == NULL);

  if (seg == NULL || seg->n_obj == params->segment_size) {
    if (seg != NULL) {
      /* set the state of the finished segment */
      seg->req_rate = params->cache_state.req_rate;
      seg->write_rate = params->cache_state.write_rate;
      seg->miss_ratio = params->cache_state.miss_ratio;
    }

    seg = allocate_new_seg(cache, bucket->bucket_id);
    append_seg_to_bucket(params, bucket, seg);
    VVERBOSE("%lu allocate new seg, %d in use seg\n", cache->n_req,
             params->n_in_use_segs);
  }

  cache_obj_t *cache_obj = &seg->objs[seg->n_obj];
  obj_init(cache, req, cache_obj, seg);
  hashtable_insert_obj(cache->hashtable, cache_obj);

  seg->n_byte += cache_obj->obj_size + cache->per_obj_overhead;
  seg->n_obj += 1;
  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;

  DEBUG_ASSERT(cache->n_obj > (params->n_in_use_segs - params->n_used_buckets) *
                                  params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_in_use_segs * params->segment_size);
}

void GLCache_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj) {
  GLCache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  bucket_t *bucket = select_segs_to_evict(cache, params->obj_sel.segs_to_evict);
  if (bucket == NULL) {
    // this can happen when space is fragmented between buckets and we cannot
    // merge and we evict segs[0] and return
    segment_t *seg = params->obj_sel.segs_to_evict[0];

    static int64_t last_print_time = 0;
    if (params->curr_rtime - last_print_time > 3600 * 6) {
      last_print_time = params->curr_rtime;
      WARN(
          "%.2lf hour, cache size %lu MB, %d segs, evicting and cannot merge\n",
          (double)params->curr_rtime / 3600.0, cache->cache_size / 1024 / 1024,
          params->n_in_use_segs);
    }

    evict_one_seg(cache, params->obj_sel.segs_to_evict[0]);
    return;
  }

  for (int i = 0; i < params->n_merge; i++) {
    params->cache_state.n_evicted_bytes +=
        params->obj_sel.segs_to_evict[i]->n_byte;
  }
  params->n_evictions += 1;

  GLCache_merge_segs(cache, bucket, params->obj_sel.segs_to_evict);

#ifdef USE_LHD
  if (params->obj_score_type == OBJ_SCORE_HIT_DENSITY
#if LHD_USE_VTIME
      && params->curr_vtime - params->last_hit_prob_compute_vtime >
             HIT_PROB_COMPUTE_INTVL) {
#else
      && params->curr_rtime - params->last_hit_prob_compute_rtime >
             HIT_PROB_COMPUTE_INTVLR) {
#endif
    /* update hit prob for all buckets */
    for (int i = 0; i < MAX_N_BUCKET; i++) {
      update_hit_prob_cdf(&params->buckets[i]);
    }
    params->last_hit_prob_compute_vtime = params->curr_vtime;
    params->last_hit_prob_compute_rtime = params->curr_rtime;
  }
#endif
}

void GLCache_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  GLCache_params_t *params = cache->eviction_params;
  abort();
}

void GLCache_remove(cache_t *cache, const obj_id_t obj_id) { abort(); }

#ifdef __cplusplus
}
#endif

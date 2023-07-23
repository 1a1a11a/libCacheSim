//
//  a learned log-structure cache module
//
//

#include <assert.h>
#include <stdbool.h>

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"
#include "GLCacheInternal.h"
#include "cacheState.h"
#include "const.h"
#include "obj.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* output file for comparing online and offline calculated segment utility */
FILE *ofile_cmp_y = NULL;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void GLCache_free(cache_t *cache);
static bool GLCache_get(cache_t *cache, const request_t *req);
static cache_obj_t *GLCache_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *GLCache_insert(cache_t *cache, const request_t *req);
static cache_obj_t *GLCache_to_evict(cache_t *cache, const request_t *req);
static void GLCache_evict(cache_t *cache, const request_t *req);
static bool GLCache_remove(cache_t *cache, const obj_id_t obj_id);

static void set_default_params(GLCache_params_t *params) {
  params->segment_size = 100;
  params->n_merge = 2;
  params->rank_intvl = 0.02;
  params->merge_consecutive_segs = true;
  params->retrain_intvl = 86400;
  params->train_source_y = TRAIN_Y_FROM_ONLINE;
  params->type = LOGCACHE_LEARNED;

  params->curr_evict_bucket_idx = 0;
  params->start_rtime = -1;
}

const char *GLCache_default_params(void) {
  return "segment-size=100, n-merge=2, "
         "type=learned, rank-intvl=0.02,"
         "merge-consecutive-segs=true, train-source-y=online,"
         "retrain-intvl=86400";
}

static void GLCache_parse_init_params(const char *cache_specific_params,
                                      GLCache_params_t *params) {
  char *params_str = strdup(cache_specific_params);

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }
    if (strcasecmp(key, "segment-size") == 0) {
      params->segment_size = atoi(value);
    } else if (strcasecmp(key, "n-merge") == 0) {
      params->n_merge = atoi(value);
    } else if (strcasecmp(key, "rank-intvl") == 0) {
      params->rank_intvl = atof(value);
    } else if (strcasecmp(key, "merge-consecutive-segs") == 0) {
      params->merge_consecutive_segs = atoi(value);
    } else if (strcasecmp(key, "retrain-intvl") == 0) {
      params->retrain_intvl = atoi(value);
    } else if (strcasecmp(key, "train-source-y") == 0) {
      if (strcasecmp(value, "online") == 0) {
        params->train_source_y = TRAIN_Y_FROM_ONLINE;
      } else if (strcasecmp(value, "oracle") == 0) {
        params->train_source_y = TRAIN_Y_FROM_ORACLE;
      } else {
        ERROR("Unknown train-source-y %s, support online/oracle\n", value);
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
        params->type = LOGCACHE_TWO_ORACLE;
      } else {
        ERROR(
            "Unknown type %s, support "
            "learned/logOracle/itemOracle/twoOracle\n",
            value);
        exit(1);
      }
    } else if (strcasecmp(key, "print") == 0 ||
               strcasecmp(key, "default") == 0) {
      printf("default params: %s\n", GLCache_default_params());
      exit(0);
    } else {
      ERROR("GLCache does not have parameter %s\n", key);
      printf("default params: %s\n", GLCache_default_params());
      exit(1);
    }
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 */
cache_t *GLCache_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GLCache", ccache_params, cache_specific_params);

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 2 + 1 + 8;  // freq, bool, history
  } else {
    cache->obj_md_size = 0;
  }

  // tells hash table that the cache_obj does not need to be free when removed
  // from the hash table
  cache->hashtable->external_obj = true;

  GLCache_params_t *params = my_malloc(GLCache_params_t);
  memset(params, 0, sizeof(GLCache_params_t));
  cache->eviction_params = params;

  set_default_params(params);

  if (cache_specific_params != NULL)
    GLCache_parse_init_params(cache_specific_params, params);

  check_params(params);

  params->n_retain_per_seg = params->segment_size / params->n_merge;

  switch (params->type) {
    case LOGCACHE_LOG_ORACLE:
      params->obj_score_type = OBJ_SCORE_AGE_BYTE;
      memcpy(cache->cache_name, "GLCache-logOracle", 17);
      break;
    case LOGCACHE_LEARNED:
      params->obj_score_type = OBJ_SCORE_AGE_BYTE;
      break;
    case LOGCACHE_ITEM_ORACLE:
      memcpy(cache->cache_name, "GLCache-itemOracle", 17);
      params->obj_score_type = OBJ_SCORE_ORACLE;
      break;
    case LOGCACHE_TWO_ORACLE:
      memcpy(cache->cache_name, "GLCache-twoOracle", 17);
      params->obj_score_type = OBJ_SCORE_ORACLE;
      break;
    default:
      ERROR("Unknown type %d\n", params->type);
      abort();
  };

  init_global_params();
  init_seg_sel(cache);
  init_obj_sel(cache);
  init_learner(cache);
  init_cache_state(cache);

  cache->cache_init = GLCache_init;
  cache->cache_free = GLCache_free;
  cache->get = GLCache_get;
  cache->find = GLCache_find;
  cache->insert = GLCache_insert;
  cache->to_evict = NULL;
  cache->evict = GLCache_evict;
  cache->remove = GLCache_remove;

  INFO(
      "%s, %.0lfMB, segment_size %d, training_interval %d, source %d, "
      "rank interval %.2lf, merge consecutive segments %d, "
      "merge %d segments\n",
      GLCache_type_names[params->type], (double)cache->cache_size / 1048576.0,
      params->segment_size, params->retrain_intvl, params->train_source_y,
      params->rank_intvl, params->merge_consecutive_segs, params->n_merge);
  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void GLCache_free(cache_t *cache) {
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

/**
 * @brief this function is the user facing API
 * it performs the following logic
 *
 * ```
 * if obj in cache:
 *    update_metadata
 *    return true
 * else:
 *    if cache does not have enough space:
 *        evict until it has space to insert
 *    insert the object
 *    return false
 * ```
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */
static bool GLCache_get(cache_t *cache, const request_t *req) {
  GLCache_params_t *params = cache->eviction_params;

  bool ret = cache_get_base(cache, req);

  if (params->type == LOGCACHE_LEARNED ||
      params->type == LOGCACHE_ITEM_ORACLE) {
    /* generate training data by taking a snapshot */
    learner_t *l = &params->learner;
    if (l->last_train_rtime > 0 &&
        params->curr_rtime - l->last_train_rtime >= params->retrain_intvl + 1) {
      train(cache);
      snapshot_segs_to_training_data(cache);
    }
  }

  update_cache_state(cache, req, ret);

  return ret;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *GLCache_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  GLCache_params_t *params = cache->eviction_params;

  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    return cache_obj;
  }

  if (!update_cache) {
    assert(0);
  }

  cache_obj_t *ret = NULL;

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
      ret = cache_obj;

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

  return ret;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *GLCache_insert(cache_t *cache, const request_t *req) {
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

  seg->n_byte += cache_obj->obj_size + cache->obj_md_size;
  seg->n_obj += 1;
  cache->occupied_byte += cache_obj->obj_size + cache->obj_md_size;
  cache->n_obj += 1;

  DEBUG_ASSERT(cache->n_obj > (params->n_in_use_segs - params->n_used_buckets) *
                                  params->segment_size);
  DEBUG_ASSERT(cache->n_obj <= params->n_in_use_segs * params->segment_size);

  return cache_obj;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void GLCache_evict(cache_t *cache, const request_t *req) {
  GLCache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  if (l->n_train == -1) {
    snapshot_segs_to_training_data(cache);
    l->last_train_rtime = params->curr_rtime;
    l->n_train = 0;
  }

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
}

void GLCache_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  GLCache_params_t *params = cache->eviction_params;
  abort();
}

bool GLCache_remove(cache_t *cache, const obj_id_t obj_id) {
  abort();
  return true;
}

#ifdef __cplusplus
}
#endif

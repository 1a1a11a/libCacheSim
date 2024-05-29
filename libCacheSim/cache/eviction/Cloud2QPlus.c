//
//  10% small FIFO + 90% main FIFO (2-bit Clock) + ghost
//  insert to small FIFO if not in the ghost, else insert to the main FIFO
//  evict from small FIFO:
//      if object in the small is accessed,
//          reinsert to main FIFO,
//      else
//          evict and insert to the ghost
//  evict from main FIFO:
//      if object in the main is accessed,
//          reinsert to main FIFO,
//      else
//          evict
//
//
//  Cloud2QPlus.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/22.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *fifo;
  cache_t *fifo_ghost;
  cache_t *main_cache;
  bool hit_on_ghost;

  int64_t n_obj_admit_to_fifo;
  int64_t n_obj_admit_to_main;
  int64_t n_obj_move_to_main;
  int64_t n_byte_admit_to_fifo;
  int64_t n_byte_admit_to_main;
  int64_t n_byte_move_to_main;

  int move_to_main_threshold;
  double fifo_size_ratio;
  double ghost_size_ratio;
  double corr_window_ratio;
  // used to indicate whether the cache is full
  bool has_evicted;
  char main_cache_type[32];

  request_t *req_local;
} Cloud2QPlus_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "fifo-size-ratio=0.10,ghost-size-ratio=0.90,move-to-main-threshold=2,"
    "corr-window-ratio=0.5";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *Cloud2QPlus_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params);
static void Cloud2QPlus_free(cache_t *cache);
static bool Cloud2QPlus_get(cache_t *cache, const request_t *req);

static cache_obj_t *Cloud2QPlus_find(cache_t *cache, const request_t *req,
                                     const bool update_cache);
static cache_obj_t *Cloud2QPlus_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Cloud2QPlus_to_evict(cache_t *cache, const request_t *req);
static void Cloud2QPlus_evict(cache_t *cache, const request_t *req);
static bool Cloud2QPlus_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t Cloud2QPlus_get_occupied_byte(const cache_t *cache);
static inline int64_t Cloud2QPlus_get_n_obj(const cache_t *cache);
static inline bool Cloud2QPlus_can_insert(cache_t *cache, const request_t *req);
static void Cloud2QPlus_parse_params(cache_t *cache,
                                     const char *cache_specific_params);

static void Cloud2QPlus_evict_fifo(cache_t *cache, const request_t *req);
static void Cloud2QPlus_evict_main(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *Cloud2QPlus_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("Cloud2QPlus", ccache_params, cache_specific_params);
  cache->cache_init = Cloud2QPlus_init;
  cache->cache_free = Cloud2QPlus_free;
  cache->get = Cloud2QPlus_get;
  cache->find = Cloud2QPlus_find;
  cache->insert = Cloud2QPlus_insert;
  cache->evict = Cloud2QPlus_evict;
  cache->remove = Cloud2QPlus_remove;
  cache->to_evict = Cloud2QPlus_to_evict;
  cache->get_n_obj = Cloud2QPlus_get_n_obj;
  cache->get_occupied_byte = Cloud2QPlus_get_occupied_byte;
  cache->can_insert = Cloud2QPlus_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(Cloud2QPlus_params_t));
  memset(cache->eviction_params, 0, sizeof(Cloud2QPlus_params_t));
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;
  params->corr_window_ratio = 0.2;
  params->has_evicted = false;

  Cloud2QPlus_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    Cloud2QPlus_parse_params(cache, cache_specific_params);
  }

  int64_t fifo_cache_size =
      (int64_t)ccache_params.cache_size * params->fifo_size_ratio;
  int64_t main_cache_size = ccache_params.cache_size - fifo_cache_size;
  int64_t fifo_ghost_cache_size =
      (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = fifo_cache_size;
  params->fifo = FIFO_init(ccache_params_local, NULL);

  if (fifo_ghost_cache_size > 0) {
    ccache_params_local.cache_size = fifo_ghost_cache_size;
    params->fifo_ghost = FIFO_init(ccache_params_local, NULL);
    snprintf(params->fifo_ghost->cache_name, CACHE_NAME_ARRAY_LEN,
             "FIFO-ghost");
  } else {
    params->fifo_ghost = NULL;
  }

  ccache_params_local.cache_size = main_cache_size;
  params->main_cache = FIFO_init(ccache_params_local, NULL);

#if defined(TRACK_EVICTION_V_AGE)
  if (params->fifo_ghost != NULL) {
    params->fifo_ghost->track_eviction_age = false;
  }
  params->fifo->track_eviction_age = false;
  params->main_cache->track_eviction_age = false;
#endif

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "Cloud2QPlus-%.4lf-%d-%.2lf", params->fifo_size_ratio,
           params->move_to_main_threshold, params->corr_window_ratio);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Cloud2QPlus_free(cache_t *cache) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->fifo->cache_free(params->fifo);
  if (params->fifo_ghost != NULL) {
    params->fifo_ghost->cache_free(params->fifo_ghost);
  }
  params->main_cache->cache_free(params->main_cache);
  free(cache->eviction_params);
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
static bool Cloud2QPlus_get(cache_t *cache, const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->fifo->get_occupied_byte(params->fifo) +
                   params->main_cache->get_occupied_byte(params->main_cache) <=
               cache->cache_size);

  bool cache_hit = cache_get_base(cache, req);
  // printf("---------- get %ld %d\n", req->obj_id, cache_hit);

  return cache_hit;
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
static cache_obj_t *Cloud2QPlus_find(cache_t *cache, const request_t *req,
                                     const bool update_cache) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;

  // if update cache is false, we only check the fifo and main caches
  if (!update_cache) {
    cache_obj_t *obj = params->fifo->find(params->fifo, req, false);
    if (obj != NULL) {
      return obj;
    }
    obj = params->main_cache->find(params->main_cache, req, false);
    if (obj != NULL) {
      return obj;
    }
    return NULL;
  }

  /* update cache is true from now */
  params->hit_on_ghost = false;
  cache_obj_t *obj = params->fifo->find(params->fifo, req, true);

  if (obj != NULL) {
    int time_since_insertion =
        params->n_obj_admit_to_fifo - obj->Cloud2QPlus.insertion_time;
    if (time_since_insertion >=
        params->fifo->cache_size * params->corr_window_ratio) {
      obj->Cloud2QPlus.freq += 1;
    }
    return obj;
  }

  if (params->fifo_ghost != NULL &&
      params->fifo_ghost->remove(params->fifo_ghost, req->obj_id)) {
    // if object in fifo_ghost, remove will return true
    params->hit_on_ghost = true;
  }

  obj = params->main_cache->find(params->main_cache, req, true);
  if (obj != NULL) {
    obj->Cloud2QPlus.freq += 1;
  }

  return obj;
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
static cache_obj_t *Cloud2QPlus_insert(cache_t *cache, const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    // printf("---- insert main %ld\n", req->obj_id);
    /* insert into the main */
    params->hit_on_ghost = false;
    params->n_obj_admit_to_main += 1;
    params->n_byte_admit_to_main += req->obj_size;
    obj = params->main_cache->insert(params->main_cache, req);
  } else {
    // printf("---- insert fifo %ld\n", req->obj_id);
    /* insert into the fifo */
    if (req->obj_size >= params->fifo->cache_size) {
      return NULL;
    }
    /* if the cache is not full, but small fifo is full
     * insert to the main fifo, this happens during warmup */
    if (!params->has_evicted && params->fifo->get_occupied_byte(params->fifo) >=
                                    params->fifo->cache_size) {
      params->n_obj_admit_to_main += 1;
      params->n_byte_admit_to_main += req->obj_size;
      obj = params->main_cache->insert(params->main_cache, req);
    } else {
      params->n_obj_admit_to_fifo += 1;
      params->n_byte_admit_to_fifo += req->obj_size;
      obj = params->fifo->insert(params->fifo, req);
      obj->Cloud2QPlus.insertion_time = params->n_obj_admit_to_fifo;
    }
  }

#if defined(TRACK_EVICTION_V_AGE)
  obj->create_time = CURR_TIME(cache, req);
#endif

#if defined(TRACK_DEMOTION)
  obj->create_time = cache->n_req;
#endif

  obj->Cloud2QPlus.freq == 0;

  return obj;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *Cloud2QPlus_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
}

static void Cloud2QPlus_evict_fifo(cache_t *cache, const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  bool has_evicted = false;
  while (!has_evicted && fifo->get_occupied_byte(fifo) > 0) {
    // evict from FIFO
    cache_obj_t *obj_to_evict = fifo->to_evict(fifo, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    // need to copy the object before it is evicted
    copy_cache_obj_to_request(params->req_local, obj_to_evict);

    if (obj_to_evict->Cloud2QPlus.freq >= params->move_to_main_threshold) {
      // freq is updated in cache_find_base
      params->n_obj_move_to_main += 1;
      params->n_byte_move_to_main += obj_to_evict->obj_size;

      cache_obj_t *new_obj = main->insert(main, params->req_local);
      // printf("---- reinsert fifo %ld\n", new_obj->obj_id);

      new_obj->misc.freq = obj_to_evict->misc.freq;
    } else {
      // insert to ghost
      if (ghost != NULL) {
        ghost->get(ghost, params->req_local);
        // printf("---- evict fifo %ld\n", params->req_local->obj_id);
      }
      has_evicted = true;
    }

    // remove from fifo, but do not update stat
    bool removed = fifo->remove(fifo, params->req_local->obj_id);
    assert(removed);
  }
}

static void Cloud2QPlus_evict_main(cache_t *cache, const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  // evict from main cache
  bool has_evicted = false;
  while (!has_evicted && main->get_occupied_byte(main) > 0) {
    cache_obj_t *obj_to_evict = main->to_evict(main, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    int freq = obj_to_evict->Cloud2QPlus.freq;
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    if (freq >= 1) {
      // we need to evict first because the object to insert has the same obj_id
      // main->evict(main, req);
      main->remove(main, obj_to_evict->obj_id);
      obj_to_evict = NULL;

      cache_obj_t *new_obj = main->insert(main, params->req_local);
      // clock with 2-bit counter
      new_obj->Cloud2QPlus.freq = MIN(freq, 3) - 1;
      new_obj->misc.freq = freq;
      // printf("---- reinsert main %ld\n", params->req_local->obj_id);
    } else {
      // main->evict(main, req);
      // printf("---- evict main %ld\n", obj_to_evict->obj_id);
      bool removed = main->remove(main, obj_to_evict->obj_id);
      if (!removed) {
        ERROR("cannot remove obj %ld\n", obj_to_evict->obj_id);
      }

      has_evicted = true;
    }
  }
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
static void Cloud2QPlus_evict(cache_t *cache, const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;

  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  params->has_evicted = true;
  if (main->get_occupied_byte(main) > main->cache_size ||
      fifo->get_occupied_byte(fifo) == 0) {
    return Cloud2QPlus_evict_main(cache, req);
  }
  return Cloud2QPlus_evict_fifo(cache, req);
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool Cloud2QPlus_remove(cache_t *cache, const obj_id_t obj_id) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->fifo->remove(params->fifo, obj_id);
  removed = removed || (params->fifo_ghost &&
                        params->fifo_ghost->remove(params->fifo_ghost, obj_id));
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t Cloud2QPlus_get_occupied_byte(const cache_t *cache) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  return params->fifo->get_occupied_byte(params->fifo) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t Cloud2QPlus_get_n_obj(const cache_t *cache) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;
  return params->fifo->get_n_obj(params->fifo) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool Cloud2QPlus_can_insert(cache_t *cache,
                                          const request_t *req) {
  Cloud2QPlus_params_t *params = (Cloud2QPlus_params_t *)cache->eviction_params;

  return req->obj_size <= params->fifo->cache_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *Cloud2QPlus_current_params(Cloud2QPlus_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128,
           "fifo-size-ratio=%.4lf,ghost-size-ratio=%.4lf,move-to-main-"
           "threshold=%d,corr-window-ratio=%.4lf\n",
           params->fifo_size_ratio, params->ghost_size_ratio,
           params->move_to_main_threshold, params->corr_window_ratio);
  return params_str;
}

static void Cloud2QPlus_parse_params(cache_t *cache,
                                     const char *cache_specific_params) {
  Cloud2QPlus_params_t *params =
      (Cloud2QPlus_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "fifo-size-ratio") == 0) {
      params->fifo_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "corr-window-ratio") == 0) {
      params->corr_window_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", Cloud2QPlus_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

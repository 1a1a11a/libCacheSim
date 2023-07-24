//
//  Quick demotion + lazy promoition v2
//
//  FIFO + Clock
//  the ratio of FIFO is decided dynamically
//  based on the marginal hits on FIFO-ghost and main cache
//  we track the hit distribution of FIFO-ghost and main cache
//  if the hit distribution of FIFO-ghost at pos 0 is larger than
//  the hit distribution of main cache at pos -1,
//  we increase FIFO size by 1
//
//
//  S3FIFOd.c
//  libCacheSim
//
//  Created by Juncheng on 1/24/23
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
  int move_to_main_threshold;

  double fifo_size_ratio;
  char main_cache_type[32];

  cache_t *fifo_eviction;
  cache_t *main_cache_eviction;
  int32_t fifo_eviction_hit;
  int32_t main_eviction_hit;

  request_t *req_local;
} S3FIFOd_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "fifo-size-ratio=0.10,main-cache=Clock2,move-to-main-threshold=1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *S3FIFOd_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);
static void S3FIFOd_free(cache_t *cache);
static bool S3FIFOd_get(cache_t *cache, const request_t *req);

static cache_obj_t *S3FIFOd_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *S3FIFOd_insert(cache_t *cache, const request_t *req);
static cache_obj_t *S3FIFOd_to_evict(cache_t *cache, const request_t *req);
static void S3FIFOd_evict(cache_t *cache, const request_t *req);
static bool S3FIFOd_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t S3FIFOd_get_occupied_byte(const cache_t *cache);
static inline int64_t S3FIFOd_get_n_obj(const cache_t *cache);
static inline bool S3FIFOd_can_insert(cache_t *cache, const request_t *req);
static void S3FIFOd_parse_params(cache_t *cache,
                                const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *S3FIFOd_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("S3FIFOd", ccache_params, cache_specific_params);
  cache->cache_init = S3FIFOd_init;
  cache->cache_free = S3FIFOd_free;
  cache->get = S3FIFOd_get;
  cache->find = S3FIFOd_find;
  cache->insert = S3FIFOd_insert;
  cache->evict = S3FIFOd_evict;
  cache->remove = S3FIFOd_remove;
  cache->to_evict = S3FIFOd_to_evict;
  cache->get_n_obj = S3FIFOd_get_n_obj;
  cache->get_occupied_byte = S3FIFOd_get_occupied_byte;
  cache->can_insert = S3FIFOd_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(S3FIFOd_params_t));
  memset(cache->eviction_params, 0, sizeof(S3FIFOd_params_t));
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;

  S3FIFOd_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    S3FIFOd_parse_params(cache, cache_specific_params);
  }

  int64_t fifo_cache_size =
      (int64_t)ccache_params.cache_size * params->fifo_size_ratio;
  int64_t main_cache_size = ccache_params.cache_size - fifo_cache_size;
  int64_t fifo_ghost_cache_size = main_cache_size;

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = fifo_cache_size;
  params->fifo = FIFO_init(ccache_params_local, NULL);

  ccache_params_local.cache_size = fifo_ghost_cache_size;
  params->fifo_ghost = FIFO_init(ccache_params_local, NULL);
  snprintf(params->fifo_ghost->cache_name, CACHE_NAME_ARRAY_LEN, "FIFO-ghost");

  ccache_params_local.cache_size = main_cache_size;
  if (strcasecmp(params->main_cache_type, "FIFO") == 0) {
    params->main_cache = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock2") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=2");
  } else if (strcasecmp(params->main_cache_type, "clock3") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=3");
  } else if (strcasecmp(params->main_cache_type, "sieve") == 0) {
    params->main_cache = Sieve_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LRU") == 0) {
    params->main_cache = LRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "ARC") == 0) {
    params->main_cache = ARC_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LHD") == 0) {
    params->main_cache = LHD_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LeCaR") == 0) {
    params->main_cache = LeCaR_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Cacheus") == 0) {
    params->main_cache = Cacheus_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "twoQ") == 0) {
    params->main_cache = TwoQ_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LIRS") == 0) {
    params->main_cache = LIRS_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Hyperbolic") == 0) {
    params->main_cache = Hyperbolic_init(ccache_params_local, NULL);
  } else {
    ERROR("S3FIFOd does not support %s \n", params->main_cache_type);
  }

  ccache_params_local.cache_size = ccache_params.cache_size / 10;
  ccache_params_local.hashpower -= 4;
  params->fifo_eviction = FIFO_init(ccache_params_local, NULL);
  params->main_cache_eviction = FIFO_init(ccache_params_local, NULL);
  snprintf(params->fifo_eviction->cache_name, CACHE_NAME_ARRAY_LEN,
           "FIFO-evicted");
  snprintf(params->main_cache_eviction->cache_name, CACHE_NAME_ARRAY_LEN, "%s",
           "main-evicted");

#if defined(TRACK_EVICTION_V_AGE)
  params->fifo->track_eviction_age = false;
  params->main_cache->track_eviction_age = false;
  params->fifo_ghost->track_eviction_age = false;
  params->fifo_eviction->track_eviction_age = false;
  params->main_cache_eviction->track_eviction_age = false;
#endif

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFOd-%s-%d",
           params->main_cache_type, params->move_to_main_threshold);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void S3FIFOd_free(cache_t *cache) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->fifo->cache_free(params->fifo);
  params->fifo_ghost->cache_free(params->fifo_ghost);
  params->main_cache->cache_free(params->main_cache);
  free(cache->eviction_params);
  cache_struct_free(cache);
}

static void S3FIFOd_update_fifo_size(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;

  int step = 20;
  step = MAX(
      1, MIN(params->fifo->cache_size, params->main_cache->cache_size) / 1000);
  bool cond1 = params->fifo_eviction_hit + params->main_eviction_hit > 100;
  bool cond2 = params->main_cache_eviction->get_occupied_byte(
                   params->main_cache_eviction) > 0;
  if (!cond2) {
    params->fifo_eviction_hit = 0;
    params->main_eviction_hit = 0;
  }

  if (cond1 && cond2) {
    if (params->fifo_eviction_hit > params->main_eviction_hit * 2) {
      // if (params->main_cache->cache_size > step) {
      if (params->main_cache->cache_size > cache->cache_size / 100) {
        params->fifo->cache_size += step;
        params->fifo_ghost->cache_size += step;
        params->main_cache->cache_size -= step;
      }
    } else if (params->main_eviction_hit > params->fifo_eviction_hit * 2) {
      // if (params->fifo->cache_size > step) {
      if (params->fifo->cache_size > cache->cache_size / 100) {
        params->fifo->cache_size -= step;
        params->fifo_ghost->cache_size -= step;
        params->main_cache->cache_size += step;
      }
    }
    params->fifo_eviction_hit = params->fifo_eviction_hit * 0.8;
    params->main_eviction_hit = params->main_eviction_hit * 0.8;
  }
}

static void S3FIFOd_update_fifo_size2(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;

  assert(params->fifo_eviction_hit + params->main_eviction_hit <= 1);
  if (params->fifo_eviction_hit == 1 && params->main_cache->cache_size > 1) {
    params->fifo->cache_size += 1;
    params->main_cache->cache_size -= 1;
  } else if (params->main_eviction_hit == 1 && params->fifo->cache_size > 1) {
    params->main_cache->cache_size += 1;
    params->fifo->cache_size -= 1;
  }
  params->fifo_eviction_hit = 0;
  params->main_eviction_hit = 0;
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
static bool S3FIFOd_get(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->fifo->get_occupied_byte(params->fifo) +
                   params->main_cache->get_occupied_byte(params->main_cache) <=
               cache->cache_size);

  // static __thread int64_t last_print_rtime = 0;
  // if (req->clock_time - last_print_rtime >= 24 * 3600) {
  //   printf(
  //       "%ld %ld day: evictHit %d %d, fifo size %ld/%ld main size %ld/%ld, ghost "
  //       "size %ld/%ld\n",
  //       cache->n_req, req->clock_time / 86400, params->fifo_eviction_hit,
  //       params->main_eviction_hit,
  //       params->fifo->get_occupied_byte(params->fifo), params->fifo->cache_size,
  //       params->main_cache->get_occupied_byte(params->main_cache),
  //       params->main_cache->cache_size,
  //       params->fifo_ghost->get_occupied_byte(params->fifo_ghost),
  //       params->fifo_ghost->cache_size);
  //   last_print_rtime = req->clock_time;
  // }

  S3FIFOd_update_fifo_size(cache, req);

  bool cache_hit = cache_get_base(cache, req);
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
static cache_obj_t *S3FIFOd_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;

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
    return obj;
  }

  if (params->fifo_ghost->remove(params->fifo_ghost, req->obj_id)) {
    params->hit_on_ghost = true;
  }

  obj = params->main_cache->find(params->main_cache, req, update_cache);

  if (params->fifo_eviction->find(params->fifo_eviction, req, false) != NULL) {
    params->fifo_eviction->remove(params->fifo_eviction, req->obj_id);
    params->fifo_eviction_hit++;
  }

  if (params->main_cache_eviction->find(params->main_cache_eviction, req,
                                        true) != NULL) {
    params->main_cache_eviction->remove(params->main_cache_eviction,
                                        req->obj_id);
    params->main_eviction_hit++;
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
static cache_obj_t *S3FIFOd_insert(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    params->main_cache->get(params->main_cache, req);
    obj = obj = params->main_cache->find(params->main_cache, req, false);
  } else {
    /* insert into the fifo */
    obj = params->fifo->insert(params->fifo, req);
  }

  assert(obj->misc.freq == 0);

#if defined(TRACK_EVICTION_V_AGE)
  obj->create_time = CURR_TIME(cache, req);
#endif

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
static cache_obj_t *S3FIFOd_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
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
static void S3FIFOd_evict(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;

  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  if (fifo->get_occupied_byte(fifo) == 0) {
    assert(main->get_occupied_byte(main) <= cache->cache_size);
    // evict from main cache
    cache_obj_t *obj = main->to_evict(main, req);
#if defined(TRACK_EVICTION_V_AGE)
    record_eviction_age(cache, obj, CURR_TIME(cache, req) - obj->create_time);
#endif
    copy_cache_obj_to_request(params->req_local, obj);
    params->main_cache_eviction->get(params->main_cache_eviction,
                                     params->req_local);
    main->evict(main, req);
    return;
  }

  // evict from FIFO
  cache_obj_t *obj = fifo->to_evict(fifo, req);
  assert(obj != NULL);
  // need to copy the object before it is evicted
  copy_cache_obj_to_request(params->req_local, obj);

#if defined(TRACK_EVICTION_V_AGE)
  if (obj->misc.freq >= params->move_to_main_threshold) {
    // promote to main cache
    cache_obj_t *new_obj = main->insert(main, params->req_local);
    new_obj->create_time = obj->create_time;
    // evict from fifo, must be after copy eviction age
    bool removed = fifo->remove(fifo, params->req_local->obj_id);
    assert(removed);

    while (main->get_occupied_byte(main) > main->cache_size) {
      // evict from main cache
      cache_obj_t *obj = main->to_evict(main, req);
      copy_cache_obj_to_request(params->req_local, obj);
      params->main_cache_eviction->get(params->main_cache_eviction,
                                       params->req_local);
      main->evict(main, req);
    }
  } else {
    // evict from fifo, must be after copy eviction age
    bool removed = fifo->remove(fifo, params->req_local->obj_id);
    assert(removed);

    record_eviction_age(cache, obj, CURR_TIME(cache, req) - obj->create_time);
    // insert to ghost
    ghost->get(ghost, params->req_local);
    params->fifo_eviction->get(params->fifo_eviction, params->req_local);
  }

#else
  // evict from fifo
  bool removed = fifo->remove(fifo, params->req_local->obj_id);
  assert(removed);

  if (obj->misc.freq >= params->move_to_main_threshold) {
    // promote to main cache
    main->insert(main, params->req_local);

    while (main->get_occupied_byte(main) > main->cache_size) {
      // evict from main cache
      cache_obj_t *obj = main->to_evict(main, req);
      copy_cache_obj_to_request(params->req_local, obj);
      params->main_cache_eviction->get(params->main_cache_eviction,
                                       params->req_local);
      main->evict(main, req);
    }
  } else {
    // insert to ghost
    ghost->get(ghost, params->req_local);
    params->fifo_eviction->get(params->fifo_eviction, params->req_local);
  }
#endif
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
static bool S3FIFOd_remove(cache_t *cache, const obj_id_t obj_id) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->fifo->remove(params->fifo, obj_id);
  removed = removed || params->fifo_ghost->remove(params->fifo_ghost, obj_id);
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t S3FIFOd_get_occupied_byte(const cache_t *cache) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  return params->fifo->get_occupied_byte(params->fifo) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t S3FIFOd_get_n_obj(const cache_t *cache) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;
  return params->fifo->get_n_obj(params->fifo) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool S3FIFOd_can_insert(cache_t *cache, const request_t *req) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)cache->eviction_params;

  return req->obj_size <= params->fifo->cache_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *S3FIFOd_current_params(S3FIFOd_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-size-ratio=%.4lf,main-cache=%s\n",
           params->fifo_size_ratio, params->main_cache->cache_name);
  return params_str;
}

static void S3FIFOd_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  S3FIFOd_params_t *params = (S3FIFOd_params_t *)(cache->eviction_params);

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
    } else if (strcasecmp(key, "main-cache") == 0) {
      strncpy(params->main_cache_type, value, 30);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", S3FIFOd_current_params(params));
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

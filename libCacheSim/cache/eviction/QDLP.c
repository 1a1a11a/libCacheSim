//
//  Quick demotion + lazy promotion v1
//
//  20% FIFO + ARC
//  insert to ARC when evicting from FIFO
//
//
//  QDLP.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *small_cache;
  cache_t *ghost_cache;
  cache_t *main_cache;
  bool hit_on_ghost;

  int64_t n_obj_admit_to_small;
  int64_t n_obj_admit_to_main;
  int64_t n_obj_move_to_main;
  int64_t n_byte_admit_to_small;
  int64_t n_byte_admit_to_main;
  int64_t n_byte_move_to_main;

  int move_to_main_threshold;
  double small_size_ratio;
  double ghost_size_ratio;
  char main_cache_type[32];

  request_t *req_local;
} QDLP_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "fifo-size-ratio=0.10,ghost-size-ratio=0.9,main-cache=Clock2,move-to-main-"
    "threshold=1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void QDLP_free(cache_t *cache);
static bool QDLP_get(cache_t *cache, const request_t *req);

static cache_obj_t *QDLP_find(cache_t *cache, const request_t *req,
                              const bool update_cache);
static cache_obj_t *QDLP_insert(cache_t *cache, const request_t *req);
static cache_obj_t *QDLP_to_evict(cache_t *cache, const request_t *req);
static void QDLP_evict(cache_t *cache, const request_t *req);
static bool QDLP_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t QDLP_get_occupied_byte(const cache_t *cache);
static inline int64_t QDLP_get_n_obj(const cache_t *cache);
static inline bool QDLP_can_insert(cache_t *cache, const request_t *req);
static void QDLP_parse_params(cache_t *cache,
                              const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *QDLP_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("QDLP", ccache_params, cache_specific_params);
  cache->cache_init = QDLP_init;
  cache->cache_free = QDLP_free;
  cache->get = QDLP_get;
  cache->find = QDLP_find;
  cache->insert = QDLP_insert;
  cache->evict = QDLP_evict;
  cache->remove = QDLP_remove;
  cache->to_evict = QDLP_to_evict;
  cache->get_n_obj = QDLP_get_n_obj;
  cache->get_occupied_byte = QDLP_get_occupied_byte;
  cache->can_insert = QDLP_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(QDLP_params_t));
  memset(cache->eviction_params, 0, sizeof(QDLP_params_t));
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;

  QDLP_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    QDLP_parse_params(cache, cache_specific_params);
  }

  int64_t fifo_cache_size =
      (int64_t)ccache_params.cache_size * params->small_size_ratio;
  int64_t main_cache_size = ccache_params.cache_size - fifo_cache_size;
  int64_t ghost_cache_size =
      (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = fifo_cache_size;
  params->small_cache = FIFO_init(ccache_params_local, NULL);

  if (ghost_cache_size > 0) {
    ccache_params_local.cache_size = ghost_cache_size;
    params->ghost_cache = FIFO_init(ccache_params_local, NULL);
    snprintf(params->ghost_cache->cache_name, CACHE_NAME_ARRAY_LEN,
             "FIFO-ghost");
  } else {
    params->ghost_cache = NULL;
  }

  ccache_params_local.cache_size = main_cache_size;
  if (strcasecmp(params->main_cache_type, "ARC") == 0) {
    params->main_cache = ARC_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LHD") == 0) {
    params->main_cache = LHD_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "sieve") == 0) {
    params->main_cache = Sieve_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock2") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=2");
  } else if (strcasecmp(params->main_cache_type, "clock3") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=3");
  } else if (strcasecmp(params->main_cache_type, "LRU") == 0) {
    params->main_cache = LRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LeCaR") == 0) {
    params->main_cache = LeCaR_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Cacheus") == 0) {
    params->main_cache = Cacheus_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "twoQ") == 0) {
    params->main_cache = TwoQ_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "FIFO") == 0) {
    params->main_cache = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "SLRU") == 0) {
    params->main_cache = SLRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LIRS") == 0) {
    params->main_cache = LIRS_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Hyperbolic") == 0) {
    params->main_cache = Hyperbolic_init(ccache_params_local, NULL);
  } else {
    ERROR("QDLP does not support %s \n", params->main_cache_type);
  }

#if defined(TRACK_EVICTION_V_AGE)
  if (params->ghost_cache != NULL) {
    params->ghost->track_eviction_age = false;
  }
  params->small_cache->track_eviction_age = false;
  params->main_cache->track_eviction_age = false;
#endif

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLP-%.4lf-%.4lf-%s-%d",
           params->small_size_ratio, params->ghost_size_ratio,
           params->main_cache_type, params->move_to_main_threshold);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void QDLP_free(cache_t *cache) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->small_cache->cache_free(params->small_cache);
  if (params->ghost_cache != NULL) {
    params->ghost_cache->cache_free(params->ghost_cache);
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
static bool QDLP_get(cache_t *cache, const request_t *req) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->small_cache->get_occupied_byte(params->small_cache) +
                   params->main_cache->get_occupied_byte(params->main_cache) <=
               cache->cache_size);

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
static cache_obj_t *QDLP_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;

  // if update cache is false, we only check the fifo and main caches
  if (!update_cache) {
    cache_obj_t *obj =
        params->small_cache->find(params->small_cache, req, false);
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
  cache_obj_t *obj = params->small_cache->find(params->small_cache, req, true);
  if (obj != NULL) {
    return obj;
  }

  if (params->ghost_cache != NULL &&
      params->ghost_cache->remove(params->ghost_cache, req->obj_id)) {
    // if object in ghost, remove will return true
    params->hit_on_ghost = true;
  }

  obj = params->main_cache->find(params->main_cache, req, true);

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
static cache_obj_t *QDLP_insert(cache_t *cache, const request_t *req) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    params->n_obj_admit_to_main += 1;
    params->n_byte_admit_to_main += req->obj_size;
    params->main_cache->get(params->main_cache, req);
    obj = params->main_cache->find(params->main_cache, req, false);
  } else {
    /* insert into the fifo */
    if (req->obj_size >= params->small_cache->cache_size) {
      return NULL;
    }
    params->n_obj_admit_to_small += 1;
    params->n_byte_admit_to_small += req->obj_size;
    obj = params->small_cache->insert(params->small_cache, req);
  }

#if defined(TRACK_EVICTION_V_AGE)
  obj->create_time = CURR_TIME(cache, req);
#endif

  assert(obj->misc.freq == 0);

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
static cache_obj_t *QDLP_to_evict(cache_t *cache, const request_t *req) {
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
static void QDLP_evict(cache_t *cache, const request_t *req) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;

  cache_t *small_cache = params->small_cache;
  cache_t *ghost_cache = params->ghost_cache;
  cache_t *main_cache = params->main_cache;

  if (small_cache->get_occupied_byte(small_cache) == 0) {
#if defined(TRACK_EVICTION_V_AGE)
    cache_obj_t *obj = main_cache->to_evict(main_cache, req);
    record_eviction_age(cache, obj, CURR_TIME(cache, req) - obj->create_time);
#endif

    assert(main_cache->get_occupied_byte(main_cache) <= cache->cache_size);
    // evict from main cache
    main_cache->evict(main_cache, req);

    return;
  }

  // evict from FIFO
  cache_obj_t *obj = small_cache->to_evict(small_cache, req);
  assert(obj != NULL);
  // need to copy the object before it is evicted
  copy_cache_obj_to_request(params->req_local, obj);

  if (obj->misc.freq >= params->move_to_main_threshold) {
    // get will insert to and evict from main cache
    params->n_obj_move_to_main += 1;
    params->n_byte_move_to_main += obj->obj_size;

    params->main_cache->get(params->main_cache, params->req_local);
#if defined(TRACK_EVICTION_V_AGE)
    main_cache->find(main_cache, params->req_local, false)->create_time =
        obj->create_time;
  } else {
    record_eviction_age(cache, obj, CURR_TIME(cache, req) - obj->create_time);
#else
  } else {
#endif
    // insert to ghost
    if (ghost_cache != NULL) {
      ghost_cache->get(ghost_cache, params->req_local);
    }
  }

  // remove from fifo, but do not update stat
  // bool removed = fifo->remove(fifo, params->req_local->obj_id);
  small_cache->evict(small_cache, req);
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
static bool QDLP_remove(cache_t *cache, const obj_id_t obj_id) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->small_cache->remove(params->small_cache, obj_id);
  removed =
      removed || (params->ghost_cache &&
                  params->ghost_cache->remove(params->ghost_cache, obj_id));
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t QDLP_get_occupied_byte(const cache_t *cache) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  return params->small_cache->get_occupied_byte(params->small_cache) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t QDLP_get_n_obj(const cache_t *cache) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;
  return params->small_cache->get_n_obj(params->small_cache) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool QDLP_can_insert(cache_t *cache, const request_t *req) {
  QDLP_params_t *params = (QDLP_params_t *)cache->eviction_params;

  return req->obj_size <= params->small_cache->cache_size &&
         cache_can_insert_default(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *QDLP_current_params(QDLP_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-size-ratio=%.4lf,main-cache=%s\n",
           params->small_size_ratio, params->main_cache->cache_name);
  return params_str;
}

static void QDLP_parse_params(cache_t *cache,
                              const char *cache_specific_params) {
  QDLP_params_t *params = (QDLP_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

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
      params->small_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "main-cache") == 0) {
      strncpy(params->main_cache_type, value, 30);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", QDLP_current_params(params));
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

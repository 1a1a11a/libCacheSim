//
//  10% small LRU + 90% main LRU (2-bit Clock) + ghost
//  insert to small LRU if not in the ghost, else insert to the main LRU
//  evict from small LRU:
//      if object in the small is accessed,
//          reinsert to main LRU,
//      else
//          evict and insert to the ghost
//  evict from main LRU:
//      if object in the main is accessed,
//          reinsert to main LRU,
//      else
//          evict
//
//
//  S3FIFO.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/22.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *LRU;
  cache_t *LRU_ghost;
  cache_t *main_cache;
  bool hit_on_ghost;

  int move_to_main_threshold;
  bool promote_on_hit;
  double LRU_size_ratio;
  double ghost_size_ratio;
  char main_cache_type[32];

  request_t *req_local;
} S3LRU_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "LRU-size-ratio=0.10,ghost-size-ratio=0.90,promote-on-hit=1,main-cache-"
    "type=lru,move-to-main-threshold=1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void S3LRU_free(cache_t *cache);
static bool S3LRU_get(cache_t *cache, const request_t *req);

static cache_obj_t *S3LRU_find(cache_t *cache, const request_t *req,
                               const bool update_cache);
static cache_obj_t *S3LRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *S3LRU_to_evict(cache_t *cache, const request_t *req);
static void S3LRU_evict(cache_t *cache, const request_t *req);
static bool S3LRU_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t S3LRU_get_occupied_byte(const cache_t *cache);
static inline int64_t S3LRU_get_n_obj(const cache_t *cache);
static inline bool S3LRU_can_insert(cache_t *cache, const request_t *req);
static void S3LRU_parse_params(cache_t *cache,
                               const char *cache_specific_params);

static void S3LRU_evict_LRU(cache_t *cache, const request_t *req);
static void S3LRU_evict_main(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *S3LRU_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("S3LRU", ccache_params, cache_specific_params);
  cache->cache_init = S3LRU_init;
  cache->cache_free = S3LRU_free;
  cache->get = S3LRU_get;
  cache->find = S3LRU_find;
  cache->insert = S3LRU_insert;
  cache->evict = S3LRU_evict;
  cache->remove = S3LRU_remove;
  cache->to_evict = S3LRU_to_evict;
  cache->get_n_obj = S3LRU_get_n_obj;
  cache->get_occupied_byte = S3LRU_get_occupied_byte;
  cache->can_insert = S3LRU_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(S3LRU_params_t));
  memset(cache->eviction_params, 0, sizeof(S3LRU_params_t));
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;
  params->promote_on_hit = true;

  S3LRU_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    S3LRU_parse_params(cache, cache_specific_params);
  }

  int64_t LRU_cache_size =
      (int64_t)ccache_params.cache_size * params->LRU_size_ratio;
  int64_t main_cache_size = ccache_params.cache_size - LRU_cache_size;
  int64_t LRU_ghost_cache_size =
      (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = LRU_cache_size;
  // params->LRU = LRU_init(ccache_params_local, NULL);
  params->LRU = FIFO_init(ccache_params_local, NULL);

  if (LRU_ghost_cache_size > 0) {
    ccache_params_local.cache_size = LRU_ghost_cache_size;
    params->LRU_ghost = LRU_init(ccache_params_local, NULL);
    snprintf(params->LRU_ghost->cache_name, CACHE_NAME_ARRAY_LEN, "LRU-ghost");
  } else {
    params->LRU_ghost = NULL;
  }

  ccache_params_local.cache_size = main_cache_size;
  if (strcasecmp(params->main_cache_type, "lru") == 0) {
    params->main_cache = LRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=1");
  } else if (strcasecmp(params->main_cache_type, "clock2") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=2");
  } else {
    ERROR("Unknown main cache type: %s", params->main_cache_type);
    exit(1);
  }

#if defined(TRACK_EVICTION_V_AGE)
  if (params->LRU_ghost != NULL) {
    params->LRU_ghost->track_eviction_age = false;
  }
  params->LRU->track_eviction_age = false;
  params->main_cache->track_eviction_age = false;
#endif

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S3LRU-FIFO-%s-%d-%.4lf-%d",
           params->main_cache_type, params->promote_on_hit,
           params->LRU_size_ratio, params->move_to_main_threshold);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void S3LRU_free(cache_t *cache) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->LRU->cache_free(params->LRU);
  if (params->LRU_ghost != NULL) {
    params->LRU_ghost->cache_free(params->LRU_ghost);
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
static bool S3LRU_get(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->LRU->get_occupied_byte(params->LRU) +
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
static cache_obj_t *S3LRU_find(cache_t *cache, const request_t *req,
                               const bool update_cache) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;

  // if update cache is false, we only check the LRU and main caches
  if (!update_cache) {
    cache_obj_t *obj = params->LRU->find(params->LRU, req, false);
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
  cache_obj_t *obj = params->LRU->find(params->LRU, req, true);

  if (obj != NULL) {
    obj->S3FIFO.freq += 1;

    if (params->promote_on_hit &&
        obj->S3FIFO.freq >= params->move_to_main_threshold) {
      // move to main cache
#if defined(TRACK_DEMOTION)
      printf("%ld keep %ld %ld\n", cache->n_req, obj->create_time,
             obj->misc.next_access_vtime);
#endif
      params->LRU->remove(params->LRU, req->obj_id);
      params->main_cache->insert(params->main_cache, req);
    }
    return obj;
  }

  if (params->LRU_ghost != NULL &&
      params->LRU_ghost->remove(params->LRU_ghost, req->obj_id)) {
    // if object in LRU_ghost, remove will return true
    params->hit_on_ghost = true;
  }

  obj = params->main_cache->find(params->main_cache, req, true);
  if (obj != NULL) {
    obj->S3FIFO.freq += 1;
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
static cache_obj_t *S3LRU_insert(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    obj = params->main_cache->insert(params->main_cache, req);
  } else {
    /* insert into the LRU */
    if (req->obj_size > params->LRU->cache_size) {
      WARN("object size %ld larger than small cache size %ld\n",
           (long)req->obj_size, (long)params->LRU->cache_size);
      return NULL;
    }
    obj = params->LRU->insert(params->LRU, req);
  }

#if defined(TRACK_EVICTION_V_AGE)
  obj->create_time = CURR_TIME(cache, req);
#endif

#if defined(TRACK_DEMOTION)
  obj->create_time = cache->n_req;
#endif

  obj->S3FIFO.freq = 0;

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
static cache_obj_t *S3LRU_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
}

static void S3LRU_evict_LRU(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  cache_t *LRU = params->LRU;
  cache_t *ghost = params->LRU_ghost;
  cache_t *main_cache = params->main_cache;

  bool has_evicted = false;

  while (!has_evicted && LRU->get_occupied_byte(LRU) > 0) {
    cache_obj_t *obj_to_evict = LRU->to_evict(LRU, req);
    copy_cache_obj_to_request(params->req_local, obj_to_evict);

    if (!params->promote_on_hit &&
        obj_to_evict->S3FIFO.freq >= params->move_to_main_threshold) {
      // move to main cache
#if defined(TRACK_DEMOTION)
      printf("%ld keep %ld %ld\n", cache->n_req, obj_to_evict->create_time,
             obj_to_evict->misc.next_access_vtime);
#endif

      main_cache->insert(main_cache, params->req_local);
    } else {
#if defined(TRACK_DEMOTION)
      printf("%ld demote %ld %ld\n", cache->n_req, obj_to_evict->create_time,
             obj_to_evict->misc.next_access_vtime);
#endif
      has_evicted = true;
      // insert to ghost
      if (ghost != NULL) {
        ghost->get(ghost, params->req_local);
      }
    }
    bool removed = LRU->remove(LRU, params->req_local->obj_id);
    assert(removed);
  }
}

static void S3LRU_evict_main(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  // cache_t *LRU = params->LRU;
  // cache_t *ghost = params->LRU_ghost;
  cache_t *main_cache = params->main_cache;

  // evict from main cache
  bool has_evicted = false;
  while (!has_evicted && main_cache->get_occupied_byte(main_cache) > 0) {
    cache_obj_t *obj_to_evict = main_cache->to_evict(main_cache, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    bool removed = main_cache->remove(main_cache, obj_to_evict->obj_id);
    if (!removed) {
      ERROR("cannot remove obj %ld\n", (long)obj_to_evict->obj_id);
    }

    has_evicted = true;
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
static void S3LRU_evict(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;

  cache_t *LRU = params->LRU;
  cache_t *main_cache = params->main_cache;

  if (main_cache->get_occupied_byte(main_cache) > main_cache->cache_size ||
      LRU->get_occupied_byte(LRU) == 0) {
    main_cache->evict(main_cache, req);
    return;
  } else {
    S3LRU_evict_LRU(cache, req);
  }
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
static bool S3LRU_remove(cache_t *cache, const obj_id_t obj_id) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->LRU->remove(params->LRU, obj_id);
  removed = removed || (params->LRU_ghost &&
                        params->LRU_ghost->remove(params->LRU_ghost, obj_id));
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t S3LRU_get_occupied_byte(const cache_t *cache) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  return params->LRU->get_occupied_byte(params->LRU) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t S3LRU_get_n_obj(const cache_t *cache) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;
  return params->LRU->get_n_obj(params->LRU) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool S3LRU_can_insert(cache_t *cache, const request_t *req) {
  S3LRU_params_t *params = (S3LRU_params_t *)cache->eviction_params;

  return req->obj_size <= params->LRU->cache_size &&
         cache_can_insert_default(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *S3LRU_current_params(S3LRU_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "LRU-size-ratio=%.4lf,main-cache=%s\n",
           params->LRU_size_ratio, params->main_cache->cache_name);
  return params_str;
}

static void S3LRU_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  S3LRU_params_t *params = (S3LRU_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  // char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "LRU-size-ratio") == 0) {
      params->LRU_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "main-cache-type") == 0 ||
               strcasecmp(key, "main-cache") == 0) {
      strncpy(params->main_cache_type, value, 31);
      params->main_cache_type[31] = '\0';
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "promote-on-hit") == 0) {
      params->promote_on_hit = atoi(value);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", S3LRU_current_params(params));
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

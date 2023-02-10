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
//  QDLPv2.c
//  libCacheSim
//
//  Created by Juncheng on 1/24/23
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *fifo;
  cache_t *fifo_ghost;
  cache_t *main_cache;
  bool hit_on_ghost;

  double fifo_size_ratio;
  char main_cache_type[32];

  cache_t *fifo_eviction;
  cache_t *main_cache_eviction;
  int32_t fifo_eviction_hit;
  int32_t main_eviction_hit;

  request_t *req_local;
} QDLPv2_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "fifo-size-ratio=0.2,main-cache=Clock";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *QDLPv2_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);
static void QDLPv2_free(cache_t *cache);
static bool QDLPv2_get(cache_t *cache, const request_t *req);

static cache_obj_t *QDLPv2_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *QDLPv2_insert(cache_t *cache, const request_t *req);
static cache_obj_t *QDLPv2_to_evict(cache_t *cache, const request_t *req);
static void QDLPv2_evict(cache_t *cache, const request_t *req);
static bool QDLPv2_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t QDLPv2_get_occupied_byte(const cache_t *cache);
static inline int64_t QDLPv2_get_n_obj(const cache_t *cache);
static inline bool QDLPv2_can_insert(cache_t *cache, const request_t *req);
static void QDLPv2_parse_params(cache_t *cache,
                                const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *QDLPv2_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("QDLPv2", ccache_params);
  cache->cache_init = QDLPv2_init;
  cache->cache_free = QDLPv2_free;
  cache->get = QDLPv2_get;
  cache->find = QDLPv2_find;
  cache->insert = QDLPv2_insert;
  cache->evict = QDLPv2_evict;
  cache->remove = QDLPv2_remove;
  cache->to_evict = QDLPv2_to_evict;
  cache->init_params = cache_specific_params;
  cache->get_n_obj = QDLPv2_get_n_obj;
  cache->get_occupied_byte = QDLPv2_get_occupied_byte;
  cache->can_insert = QDLPv2_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(QDLPv2_params_t));
  memset(cache->eviction_params, 0, sizeof(QDLPv2_params_t));
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;

  QDLPv2_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    QDLPv2_parse_params(cache, cache_specific_params);
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
#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
  params->fifo_ghost->track_eviction_age = false;
#endif

  ccache_params_local.cache_size = main_cache_size;
  if (strcasecmp(params->main_cache_type, "FIFO") == 0) {
    params->main_cache = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock-2") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=2");
  } else if (strcasecmp(params->main_cache_type, "clock-3") == 0) {
    params->main_cache = Clock_init(ccache_params_local, "n-bit-counter=3");
  } else if (strcasecmp(params->main_cache_type, "myclock") == 0) {
    params->main_cache = MyClock_init(ccache_params_local, NULL);
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
    ERROR("QDLPv2 does not support %s \n", params->main_cache_type);
  }

  ccache_params_local.cache_size = ccache_params.cache_size / 10;
  ccache_params_local.hashpower -= 4;
  // ccache_params_local.cache_size = 100;
  params->fifo_eviction = FIFO_init(ccache_params_local, NULL);
  params->main_cache_eviction = FIFO_init(ccache_params_local, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv2-%.2lf-ghost-%s",
           params->fifo_size_ratio, params->main_cache_type);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void QDLPv2_free(cache_t *cache) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->fifo->cache_free(params->fifo);
  params->fifo_ghost->cache_free(params->fifo_ghost);
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
static bool QDLPv2_get(cache_t *cache, const request_t *req) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->fifo->get_occupied_byte(params->fifo) +
                   params->main_cache->get_occupied_byte(params->main_cache) <=
               cache->cache_size);

  static __thread int64_t last_print_rtime = 0;
  static __thread int last_change_direction = 0;
  if (cache->n_req % 1000 == 0) {
    // if (cache->n_req % 1000 == 0) {
    // if (req->clock_time - last_print_rtime >= 60) {
    //   printf("%ld: %d %d %ld %ld\n", cache->n_req, params->fifo_eviction_hit,
    //          params->main_eviction_hit, params->fifo->cache_size,
    //          params->main_cache->cache_size);
    //   last_print_rtime = req->clock_time;
    // }

    if (params->fifo_eviction_hit > params->main_eviction_hit * 1.2) {
      if (params->main_cache->cache_size > 20) {
        params->fifo->cache_size += 20;
        params->main_cache->cache_size -= 20;
      }
      last_change_direction = 1;
    } else if (params->main_eviction_hit > params->fifo_eviction_hit * 1.2) {
      if (params->fifo->cache_size > 20) {
        params->fifo->cache_size -= 20;
        params->main_cache->cache_size += 20;
      }
      last_change_direction = 2;
    }
    params->fifo_eviction_hit = params->fifo_eviction_hit * 0.8;
    params->main_eviction_hit = params->main_eviction_hit * 0.8;
  }

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
static cache_obj_t *QDLPv2_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

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
  cache_obj_t *obj = params->fifo->find(params->fifo, req, false);
  if (obj != NULL) {
    // we can use misc field because FIFO does not use any metadata
    DEBUG_ASSERT(obj->misc.q_id == 1);
    obj->misc.freq = 1;

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
                                        false) != NULL) {
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
static cache_obj_t *QDLPv2_insert(cache_t *cache, const request_t *req) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    obj = params->main_cache->insert(params->main_cache, req);
  } else {
    /* insert into the fifo */
    obj = params->fifo->insert(params->fifo, req);
    obj->misc.q_id = 1;  // 1 is fifo cache
    obj->misc.freq = 0;
  }
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
static cache_obj_t *QDLPv2_to_evict(cache_t *cache, const request_t *req) {
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
static void QDLPv2_evict(cache_t *cache, const request_t *req) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  if (fifo->get_occupied_byte(fifo) == 0) {
    assert(main->get_occupied_byte(main) > main->cache_size);
    assert(main->get_occupied_byte(main) <= cache->cache_size);
    // evict from main cache
    cache_obj_t *obj = main->to_evict(main, req);
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
  // remove from fifo1, but do not update stat
  fifo->evict(fifo, req);

  if (obj->misc.freq > 0) {
    // promote to main cache
    // get will insert to and evict from main cache
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
static bool QDLPv2_remove(cache_t *cache, const obj_id_t obj_id) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->fifo->remove(params->fifo, obj_id);
  removed = removed || params->fifo_ghost->remove(params->fifo_ghost, obj_id);
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t QDLPv2_get_occupied_byte(const cache_t *cache) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  return params->fifo->get_occupied_byte(params->fifo) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t QDLPv2_get_n_obj(const cache_t *cache) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  return params->fifo->get_n_obj(params->fifo) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool QDLPv2_can_insert(cache_t *cache, const request_t *req) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

  return req->obj_size <= params->fifo->cache_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *QDLPv2_current_params(QDLPv2_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-size-ratio=%.4lf,main-cache=%s\n",
           params->fifo_size_ratio, params->main_cache->cache_name);
  return params_str;
}

static void QDLPv2_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)(cache->eviction_params);

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
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", QDLPv2_current_params(params));
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

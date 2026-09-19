//
//  S4-FIFO: the eviction heuristic of "Learning-Augmented Heuristics: Simple,
//  yet Smart, Robust and Interpretable Cache Eviction".
//
//  S4-FIFO keeps S3-FIFO's three physical queues -- a small queue that filters
//  one-hit wonders, a main queue that retains objects which proved useful, and
//  a metadata-only ghost queue that remembers what the small queue threw away
//  -- and turns the constants S3-FIFO hard-codes into five knobs:
//
//    small-size-ratio        rho_S  0.10  fraction of the cache held by small
//    ghost-size-ratio        rho_G  0.90  ghost size, relative to cache size
//    skip-ratio              kappa  0.00  head fraction of small that does not
//                                         count hits
//    move-to-main-threshold  tau_S  2     hits in small needed to reach main
//    ghost-to-main-threshold tau_G  0     ghost hits needed to skip probation
//
//  The skip ratio is what makes this "S4": hits on objects still in the newest
//  kappa fraction of the small queue do not increment the frequency counter, so
//  a burst of correlated references right after insertion cannot buy a promotion
//  to main. That carves a virtual probationary region out of the head of the
//  small queue -- a fourth region -- without a fourth physical queue.
//
//  The ghost threshold handles cyclic scans. At tau_G = 0 a ghost hit promotes
//  straight to main, as in S3-FIFO. At tau_G = 1 the first ghost hit only sends
//  the object back through probation; the count of ghost hits rides along with
//  the object and is restored into the ghost entry if it is demoted again, so a
//  second ghost hit is what finally promotes it.
//
//  At the default parameters above, S4-FIFO reduces exactly to S3-FIFO.
//
//  The paper pairs this heuristic with an offline-trained gradient-boosted tree
//  that picks a configuration per workload. That model is not part of this
//  implementation -- only the heuristic it steers is. Pass a configuration with
//  -e, e.g. -e "small-size-ratio=0.05,skip-ratio=0.25".
//
//
//  S4FIFO.c
//  libCacheSim
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *small_fifo;
  cache_t *ghost_fifo;
  cache_t *main_fifo;

  bool hit_on_ghost;
  /* ghost hits accumulated by the object being inserted, this one included;
   * only meaningful while hit_on_ghost is set */
  int32_t ghost_hit_count;

  double small_size_ratio;
  double ghost_size_ratio;
  double skip_ratio;
  int move_to_main_threshold;
  int ghost_to_main_threshold;

  /* bytes ever inserted into the small queue. An object's distance from the
   * head is the number of bytes inserted after it, which is this counter minus
   * the value it recorded on insertion -- the skip region is where that
   * distance is still below skip_ratio * small_fifo->cache_size. Tracking bytes
   * rather than objects keeps the region a true fraction of the queue when
   * objects have different sizes. */
  int64_t small_insert_byte;

  bool has_evicted;
  request_t *req_local;
} S4FIFO_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "small-size-ratio=0.10,ghost-size-ratio=0.90,skip-ratio=0.00,"
    "move-to-main-threshold=2,ghost-to-main-threshold=0";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void S4FIFO_free(cache_t *cache);
static bool S4FIFO_get(cache_t *cache, const request_t *req);

static cache_obj_t *S4FIFO_find(cache_t *cache, const request_t *req,
                                bool update_cache);
static cache_obj_t *S4FIFO_insert(cache_t *cache, const request_t *req);
static cache_obj_t *S4FIFO_to_evict(cache_t *cache, const request_t *req);
static void S4FIFO_evict(cache_t *cache, const request_t *req);
static bool S4FIFO_remove(cache_t *cache, obj_id_t obj_id);
static inline int64_t S4FIFO_get_occupied_byte(const cache_t *cache);
static inline int64_t S4FIFO_get_n_obj(const cache_t *cache);
static inline bool S4FIFO_can_insert(cache_t *cache, const request_t *req);
static void S4FIFO_parse_params(cache_t *cache,
                                const char *cache_specific_params);

static void S4FIFO_evict_small(cache_t *cache, const request_t *req);
static void S4FIFO_evict_main(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/* Compute initial hashpower for a sub-cache of the given byte size.
 * Sizes the table to hold roughly size_bytes/8 entries (8-byte ptr per slot),
 * clamped to [1, HASH_POWER_DEFAULT]. */
static inline int s4fifo_hashpower_for_size(int64_t size_bytes) {
  if (size_bytes <= 0) return 1;
  int hp = 1;
  int64_t slots = size_bytes / 8;
  while ((1LL << hp) < slots && hp < HASH_POWER_DEFAULT) hp++;
  return hp;
}

/* As above, but never above what the caller asked for: --hashpower is
 * documented as a way to cut memory, and without this cap each sub-cache would
 * size its own table from its byte size and ignore the request entirely. A
 * hashpower of 0 is the "use HASH_POWER_DEFAULT" sentinel rather than a
 * request, so it caps nothing. */
static inline int s4fifo_child_hashpower(int requested, int64_t size_bytes) {
  int hp = s4fifo_hashpower_for_size(size_bytes);
  return (requested > 0 && requested < hp) ? requested : hp;
}

cache_t *S4FIFO_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("S4FIFO", ccache_params, cache_specific_params);
  cache->cache_init = S4FIFO_init;
  cache->cache_free = S4FIFO_free;
  cache->get = S4FIFO_get;
  cache->find = S4FIFO_find;
  cache->insert = S4FIFO_insert;
  cache->evict = S4FIFO_evict;
  cache->remove = S4FIFO_remove;
  cache->to_evict = S4FIFO_to_evict;
  cache->get_n_obj = S4FIFO_get_n_obj;
  cache->get_occupied_byte = S4FIFO_get_occupied_byte;
  cache->can_insert = S4FIFO_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(S4FIFO_params_t));
  memset(cache->eviction_params, 0, sizeof(S4FIFO_params_t));
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;
  params->ghost_hit_count = 0;
  params->small_insert_byte = 0;
  params->has_evicted = false;

  S4FIFO_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    S4FIFO_parse_params(cache, cache_specific_params);
  }

  int64_t small_fifo_size =
      (int64_t)(ccache_params.cache_size * params->small_size_ratio);
  int64_t main_fifo_size = ccache_params.cache_size - small_fifo_size;
  int64_t ghost_fifo_size =
      (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  if (small_fifo_size <= 0 || main_fifo_size <= 0) {
    ERROR(
        "Invalid cache size configuration: small_fifo=%lld bytes, "
        "main_fifo=%lld bytes\n",
        (long long)small_fifo_size, (long long)main_fifo_size);
  }

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = small_fifo_size;
  ccache_params_local.hashpower =
      s4fifo_child_hashpower(ccache_params.hashpower, small_fifo_size);
  params->small_fifo = FIFO_init(ccache_params_local, NULL);

  if (ghost_fifo_size > 0) {
    ccache_params_local.cache_size = ghost_fifo_size;
    ccache_params_local.hashpower =
        s4fifo_child_hashpower(ccache_params.hashpower, ghost_fifo_size);
    params->ghost_fifo = FIFO_init(ccache_params_local, NULL);
    snprintf(params->ghost_fifo->cache_name, CACHE_NAME_ARRAY_LEN,
             "FIFO-ghost");
  } else {
    params->ghost_fifo = NULL;
  }

  ccache_params_local.cache_size = main_fifo_size;
  ccache_params_local.hashpower =
      s4fifo_child_hashpower(ccache_params.hashpower, main_fifo_size);
  params->main_fifo = FIFO_init(ccache_params_local, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S4FIFO-%.4lf-%.2lf-%d-%d",
           params->small_size_ratio, params->skip_ratio,
           params->move_to_main_threshold, params->ghost_to_main_threshold);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void S4FIFO_free(cache_t *cache) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->small_fifo->cache_free(params->small_fifo);
  if (params->ghost_fifo != NULL) {
    params->ghost_fifo->cache_free(params->ghost_fifo);
  }
  params->main_fifo->cache_free(params->main_fifo);
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
static bool S4FIFO_get(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->small_fifo->get_occupied_byte(params->small_fifo) +
                   params->main_fifo->get_occupied_byte(params->main_fifo) <=
               cache->cache_size);

  bool cache_hit = cache_get_base(cache, req);

  return cache_hit;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/* Is the object still inside the head region of the small queue that the skip
 * ratio carves out? Hits there do not count towards promotion. */
static inline bool s4fifo_in_skip_region(const S4FIFO_params_t *params,
                                         const cache_obj_t *obj) {
  if (params->skip_ratio <= 0) {
    return false;
  }
  int64_t skip_byte =
      (int64_t)(params->small_fifo->cache_size * params->skip_ratio);
  return params->small_insert_byte - obj->S4FIFO.small_insert_byte < skip_byte;
}

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
static cache_obj_t *S4FIFO_find(cache_t *cache, const request_t *req,
                                bool update_cache) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;

  // if update cache is false, we only check the small and main caches
  if (!update_cache) {
    cache_obj_t *obj = params->small_fifo->find(params->small_fifo, req, false);
    if (obj != NULL) {
      return obj;
    }
    obj = params->main_fifo->find(params->main_fifo, req, false);
    if (obj != NULL) {
      return obj;
    }
    return NULL;
  }

  /* update cache is true from now */
  params->hit_on_ghost = false;
  params->ghost_hit_count = 0;

  cache_obj_t *obj = params->small_fifo->find(params->small_fifo, req, true);
  if (obj != NULL) {
    /* a burst of references to an object that has only just been inserted says
     * nothing about its reuse, so the head of the small queue does not count */
    if (!s4fifo_in_skip_region(params, obj)) {
      obj->S4FIFO.freq += 1;
    }
    return obj;
  }

  if (params->ghost_fifo != NULL) {
    cache_obj_t *ghost_obj =
        params->ghost_fifo->find(params->ghost_fifo, req, false);
    if (ghost_obj != NULL) {
      params->hit_on_ghost = true;
      params->ghost_hit_count = ghost_obj->S4FIFO.ghost_hits + 1;
      params->ghost_fifo->remove(params->ghost_fifo, req->obj_id);
    }
  }

  obj = params->main_fifo->find(params->main_fifo, req, true);
  if (obj != NULL) {
    obj->S4FIFO.freq += 1;
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
static cache_obj_t *S4FIFO_insert(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  cache_t *small_fifo = params->small_fifo;
  cache_t *main_fifo = params->main_fifo;

  /* ghost hits carried by this object, including the one that just happened */
  int32_t ghost_hits = 0;

  if (params->hit_on_ghost) {
    params->hit_on_ghost = false;
    if (params->ghost_hit_count > params->ghost_to_main_threshold) {
      /* enough ghost hits to skip probation: insert into main FIFO */
      obj = main_fifo->insert(main_fifo, req);
      obj->S4FIFO.freq = 0;
      obj->S4FIFO.ghost_hits = 0;
      /* only ever read for objects sitting in the small queue; 0 is the
       * fail-safe value there, reading as "long past the skip window" */
      obj->S4FIFO.small_insert_byte = 0;
      return obj;
    }
    /* not enough yet: another round of probation, carrying the count so that a
     * later ghost hit can still promote this object */
    ghost_hits = params->ghost_hit_count;
  }

  /* insert into small fifo */
  // NOTE: Inserting an object whose size equals the size of small fifo is
  // NOT allowed. Doing so would completely fill the small fifo, causing all
  // objects in small fifo to be evicted. This scenario may occur
  // when using a tiny cache size.
  if (req->obj_size >= small_fifo->cache_size) {
    return NULL;
  }

  if (!params->has_evicted &&
      small_fifo->get_occupied_byte(small_fifo) >= small_fifo->cache_size) {
    obj = main_fifo->insert(main_fifo, req);
    obj->S4FIFO.small_insert_byte = 0;
  } else {
    obj = small_fifo->insert(small_fifo, req);
    /* record the queue head as it stands once this object is in, so the
     * distance measured later is the bytes inserted strictly after it */
    params->small_insert_byte += req->obj_size;
    obj->S4FIFO.small_insert_byte = params->small_insert_byte;
  }

  obj->S4FIFO.freq = 0;
  obj->S4FIFO.ghost_hits = ghost_hits;

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
static cache_obj_t *S4FIFO_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
}

/* Demote an object to the ghost queue, preserving the ghost hits it has
 * already accumulated so that tau_G can count across several cache lifetimes.
 * ghost->get() inserts on a miss and leaves an existing entry alone, so the
 * count is written back either way. */
static void s4fifo_insert_ghost(S4FIFO_params_t *params, int32_t ghost_hits) {
  cache_t *ghost_fifo = params->ghost_fifo;
  if (ghost_fifo == NULL) {
    return;
  }

  ghost_fifo->get(ghost_fifo, params->req_local);
  cache_obj_t *ghost_obj =
      ghost_fifo->find(ghost_fifo, params->req_local, false);
  if (ghost_obj != NULL) {
    ghost_obj->S4FIFO.ghost_hits = ghost_hits;
  }
}

static void S4FIFO_evict_small(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  cache_t *small_fifo = params->small_fifo;
  cache_t *main_fifo = params->main_fifo;

  bool has_evicted = false;
  while (!has_evicted && small_fifo->get_occupied_byte(small_fifo) > 0) {
    cache_obj_t *obj_to_evict = small_fifo->to_evict(small_fifo, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    int64_t freq = obj_to_evict->S4FIFO.freq;
    int32_t ghost_hits = obj_to_evict->S4FIFO.ghost_hits;
    // need to copy the object before it is evicted
    copy_cache_obj_to_request(params->req_local, obj_to_evict);

    if (freq >= params->move_to_main_threshold) {
      cache_obj_t *new_obj = main_fifo->insert(main_fifo, params->req_local);
      new_obj->S4FIFO.freq = 0;
      new_obj->S4FIFO.ghost_hits = ghost_hits;
      new_obj->S4FIFO.small_insert_byte = 0;
    } else {
      // insert to ghost
      s4fifo_insert_ghost(params, ghost_hits);
      has_evicted = true;
    }

    // remove from small fifo, but do not update stat
    bool removed = small_fifo->remove(small_fifo, params->req_local->obj_id);
    DEBUG_ASSERT(removed);
  }
}

static void S4FIFO_evict_main(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  cache_t *main_fifo = params->main_fifo;

  bool has_evicted = false;
  while (!has_evicted && main_fifo->get_occupied_byte(main_fifo) > 0) {
    cache_obj_t *obj_to_evict = main_fifo->to_evict(main_fifo, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    int64_t freq = obj_to_evict->S4FIFO.freq;
    int32_t ghost_hits = obj_to_evict->S4FIFO.ghost_hits;
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    if (freq >= 1) {
      // we need to evict first because the object to insert has the same obj_id
      main_fifo->remove(main_fifo, obj_to_evict->obj_id);
      obj_to_evict = NULL;

      cache_obj_t *new_obj = main_fifo->insert(main_fifo, params->req_local);
      // clock with 2-bit counter
      new_obj->S4FIFO.freq = MIN(freq, 3) - 1;
      new_obj->S4FIFO.ghost_hits = ghost_hits;
      new_obj->S4FIFO.small_insert_byte = 0;
    } else {
      bool removed = main_fifo->remove(main_fifo, obj_to_evict->obj_id);
      DEBUG_ASSERT(removed);

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
static void S4FIFO_evict(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  params->has_evicted = true;

  cache_t *small_fifo = params->small_fifo;
  cache_t *main_fifo = params->main_fifo;

  if (main_fifo->get_occupied_byte(main_fifo) > main_fifo->cache_size ||
      small_fifo->get_occupied_byte(small_fifo) == 0) {
    S4FIFO_evict_main(cache, req);
  } else {
    S4FIFO_evict_small(cache, req);
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
static bool S4FIFO_remove(cache_t *cache, obj_id_t obj_id) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->small_fifo->remove(params->small_fifo, obj_id);
  removed = removed || (params->ghost_fifo &&
                        params->ghost_fifo->remove(params->ghost_fifo, obj_id));
  removed = removed || params->main_fifo->remove(params->main_fifo, obj_id);

  return removed;
}

static inline int64_t S4FIFO_get_occupied_byte(const cache_t *cache) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  return params->small_fifo->get_occupied_byte(params->small_fifo) +
         params->main_fifo->get_occupied_byte(params->main_fifo);
}

static inline int64_t S4FIFO_get_n_obj(const cache_t *cache) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  return params->small_fifo->get_n_obj(params->small_fifo) +
         params->main_fifo->get_n_obj(params->main_fifo);
}

static inline bool S4FIFO_can_insert(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;

  return req->obj_size <= params->small_fifo->cache_size &&
         cache_can_insert_default(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *S4FIFO_current_params(S4FIFO_params_t *params) {
  static __thread char params_str[256];
  snprintf(params_str, 256,
           "small-size-ratio=%.4lf,ghost-size-ratio=%.4lf,skip-ratio=%.4lf,"
           "move-to-main-threshold=%d,ghost-to-main-threshold=%d\n",
           params->small_size_ratio, params->ghost_size_ratio,
           params->skip_ratio, params->move_to_main_threshold,
           params->ghost_to_main_threshold);
  return params_str;
}

static void S4FIFO_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)(cache->eviction_params);

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

    if (strcasecmp(key, "small-size-ratio") == 0 ||
        strcasecmp(key, "fifo-size-ratio") == 0) {
      params->small_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "skip-ratio") == 0) {
      params->skip_ratio = strtod(value, NULL);
      if (params->skip_ratio < 0 || params->skip_ratio >= 1) {
        ERROR("%s skip-ratio must be in [0, 1), got %s\n", cache->cache_name,
              value);
      }
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "ghost-to-main-threshold") == 0) {
      params->ghost_to_main_threshold = atoi(value);
      if (params->ghost_to_main_threshold < 0) {
        ERROR("%s ghost-to-main-threshold must be non-negative, got %s\n",
              cache->cache_name, value);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", S4FIFO_current_params(params));
      free(old_params_str);
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

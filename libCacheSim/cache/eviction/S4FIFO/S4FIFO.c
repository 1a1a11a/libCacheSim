//
//  S4FIFO: S3-FIFO generalized with two extra tunable knobs so that its
//  small-queue promotion and ghost-queue promotion policies can be tuned
//  per-workload instead of using S3-FIFO's fixed defaults.
//
//  S4FIFO is the cache-eviction heuristic used as the "data plane" in
//  "Learning-Augmented Heuristics: Simple, yet Smart, Robust and
//  Interpretable Cache Eviction" (OSDI 2026). The paper's control plane
//  learns good values for this heuristic's parameters from cache-level
//  features collected offline; this file only implements the heuristic
//  itself, so S4FIFO can be used exactly like any other static eviction
//  algorithm (e.g. s3fifo) via `cache_specific_params`.
//
//  S4FIFO has the same 10% small FIFO + 90% main FIFO (2-bit Clock) +
//  ghost FIFO structure as S3FIFO.c, plus two additional knobs:
//
//    - ghost-to-main-threshold: S3-FIFO promotes an object straight to the
//      main FIFO the first time it is re-requested while in the ghost
//      queue (threshold <= 0). With a positive threshold, a ghost hit
//      never fast-tracks straight to main - the object is simply
//      re-admitted to the small FIFO instead, like any other miss.
//
//    - small-skip-ratio: S3-FIFO counts every re-request to an object
//      still in the small FIFO as a "hit" that counts towards promotion.
//      S4FIFO instead ignores re-requests to objects that were inserted
//      into the small FIFO very recently (within the most recent
//      `small-skip-ratio` fraction of the small FIFO's current object
//      count), which creates a virtual "probationary" region at the tail
//      of the small FIFO and filters out quick re-requests (e.g. from
//      scans) that would otherwise look like genuine reuse.
//
//  With the defaults below (ghost-to-main-threshold=0, small-skip-ratio=0)
//  both knobs are no-ops and S4FIFO behaves identically to S3FIFO.
//
//  S4FIFO.c
//  libCacheSim
//

#include "S4FIFO_features.h"
#include "S4FIFO_predictor.h"
#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  S4FIFO_PHASE_WARMUP = 0,
  S4FIFO_PHASE_FEATURE_COLLECT,
  S4FIFO_PHASE_PREDICTING,
} S4FIFO_phase_t;

typedef struct {
  cache_t *small_fifo;
  cache_t *ghost_fifo;
  cache_t *main_fifo;
  bool hit_on_ghost;

  int move_to_main_threshold;   // small/main queue: hits needed to promote
  int ghost_to_main_threshold;  // ghost queue: hits needed to promote
  double small_size_ratio;
  double ghost_size_ratio;
  double small_skip_ratio;  // fraction of small FIFO treated as probation

  bool has_evicted;
  request_t *req_local;
  int64_t small_insert_seq;  // counts inserts into the small FIFO

  // learned control plane: collect features for `feature_collect_reqs`
  // requests, predict a configuration, apply it, and (if
  // `prediction_interval` > 0) repeat every `prediction_interval` requests.
  bool auto_tune;
  S4FIFO_phase_t phase;
  int64_t feature_collect_reqs;
  int64_t n_total_req;  // 0 if the caller didn't supply one
  int64_t prediction_interval;
  int64_t phase_start_n_req;  // cache->n_req when the current phase began
  int64_t main_insert_seq;    // counts inserts into the main FIFO
  int64_t ghost_insert_seq;   // counts inserts into the ghost FIFO
  // set when find() saw this request in the ghost FIFO but did not promote
  // it straight to main (ghost_to_main_threshold > 0) - insert() uses this
  // to avoid double-counting the object as "unique" when it's re-admitted
  // to the small FIFO.
  bool seen_in_ghost_not_promoted;
  S4FIFO_feature_collector_t *feature_collector;
  // empty (the default) means the compiled-in model
  char model_path[256];
  // log the model's feature vector where a prediction would be made, for
  // gathering training data
  bool dump_features;
} S4FIFO_params_t;

// auto-tune is on by default, observing the first 20% of the trace - the
// protocol the shipped model was trained for. auto-tune=0 gives the static
// heuristic, which is S3-FIFO.
static const char *DEFAULT_CACHE_PARAMS =
    "small-size-ratio=0.10,ghost-size-ratio=0.90,move-to-main-threshold=2,"
    "ghost-to-main-threshold=0,small-skip-ratio=0.00,auto-tune=1,"
    "feature-collect-reqs=0.2,prediction-interval=0";

// used when a fractional feature-collect-reqs can't be resolved
#define S4FIFO_FEATURE_COLLECT_REQS_FALLBACK 10000

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

static void S4FIFO_update_phase(cache_t *cache, S4FIFO_params_t *params);
static void S4FIFO_apply_predicted_config(cache_t *cache,
                                          S4FIFO_params_t *params);
static inline bool S4FIFO_is_collecting(const S4FIFO_params_t *params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

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
  // must precede parse_params, which resolves a fractional
  // feature-collect-reqs against it
  params->n_total_req = ccache_params.n_total_req;

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
        "main_fifo=%lld "
        "bytes\n",
        (long long)small_fifo_size, (long long)main_fifo_size);
  }

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = small_fifo_size;
  params->small_fifo = FIFO_init(ccache_params_local, NULL);
  params->has_evicted = false;

  if (ghost_fifo_size > 0) {
    ccache_params_local.cache_size = ghost_fifo_size;
    params->ghost_fifo = FIFO_init(ccache_params_local, NULL);
    snprintf(params->ghost_fifo->cache_name, CACHE_NAME_ARRAY_LEN,
             "FIFO-ghost");
  } else {
    params->ghost_fifo = NULL;
  }

  ccache_params_local.cache_size = main_fifo_size;
  params->main_fifo = FIFO_init(ccache_params_local, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S4FIFO-%.4lf-%d-%d-%.2lf",
           params->small_size_ratio, params->move_to_main_threshold,
           params->ghost_to_main_threshold, params->small_skip_ratio);

  params->phase = S4FIFO_PHASE_WARMUP;
  params->phase_start_n_req = 0;
  if (params->auto_tune) {
    params->feature_collector = malloc(sizeof(S4FIFO_feature_collector_t));
    S4FIFO_feature_collector_init(
        params->feature_collector, ccache_params.cache_size,
        params->small_fifo->cache_size, params->main_fifo->cache_size,
        params->ghost_fifo ? params->ghost_fifo->cache_size : 0,
        S4FIFO_FEATURE_DEFAULT_BUCKETS, params->feature_collect_reqs / 2);
  } else {
    params->feature_collector = NULL;
  }

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
  if (params->feature_collector != NULL) {
    free(params->feature_collector);
  }
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

  if (params->feature_collector != NULL) {
    S4FIFO_update_phase(cache, params);
    if (S4FIFO_is_collecting(params)) {
      S4FIFO_feature_collector_record_request(params->feature_collector);
    }
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
 * @brief return the number of small-FIFO inserts (since cache creation) that
 * happened strictly before `obj`'s own insert, i.e. how many objects have
 * been inserted into the small FIFO after `obj` was. Because the small FIFO
 * evicts in strict insertion order, this is exactly `obj`'s distance from the
 * tail of the small FIFO, measured in objects rather than bytes.
 */
static inline int64_t S4FIFO_small_fifo_age(const S4FIFO_params_t *params,
                                            const cache_obj_t *obj) {
  return params->small_insert_seq - obj->S4FIFO.insert_seq;
}

// ---- learned control plane: feature-tracking helpers -----------------
// These are all no-ops unless auto-tune is enabled and currently in the
// FEATURE_COLLECT phase; centralizing that check here (rather than
// repeating it at every call site) is what keeps S4FIFO_find/_insert/
// _evict_small readable.

static inline bool S4FIFO_is_collecting(const S4FIFO_params_t *params) {
  return params->feature_collector != NULL &&
         params->phase == S4FIFO_PHASE_FEATURE_COLLECT;
}

// Records `obj`'s insertion into the small FIFO and stamps its insert_seq
// (used regardless of auto-tune by S4FIFO_small_fifo_age, so this only
// skips the feature-collector call, not the stamp, when not collecting).
static inline void S4FIFO_track_small_insert(S4FIFO_params_t *params,
                                             cache_obj_t *obj) {
  obj->S4FIFO.insert_seq = params->small_insert_seq;
  if (S4FIFO_is_collecting(params)) {
    S4FIFO_feature_collector_record_insert_small(params->feature_collector,
                                                 params->small_insert_seq);
    if (!params->seen_in_ghost_not_promoted) {
      S4FIFO_feature_collector_record_unique(params->feature_collector);
    }
  }
  params->small_insert_seq++;
}

// Same, for an object landing in the main FIFO (ghost promotion,
// cold-start bypass, or small-queue promotion - all three call this).
static inline void S4FIFO_track_main_insert(S4FIFO_params_t *params,
                                            cache_obj_t *obj) {
  if (obj == NULL) return;
  obj->S4FIFO.insert_seq = params->main_insert_seq;
  if (S4FIFO_is_collecting(params)) {
    S4FIFO_feature_collector_record_insert_main(params->feature_collector,
                                                params->main_insert_seq);
  }
  params->main_insert_seq++;
}

// Records a small-FIFO object falling out into the ghost FIFO: the ghost
// insert itself (`ghost_fifo->get`) always happens; the insert_seq/bucket
// stamp and one-hit-wonder count are feature-collection bookkeeping only.
static inline void S4FIFO_track_ghost_insert(S4FIFO_params_t *params,
                                             cache_t *ghost_fifo,
                                             request_t *req_local) {
  ghost_fifo->get(ghost_fifo, req_local);
  if (!S4FIFO_is_collecting(params)) return;

  cache_obj_t *ghost_obj = ghost_fifo->find(ghost_fifo, req_local, false);
  if (ghost_obj != NULL) {
    ghost_obj->S4FIFO.insert_seq = params->ghost_insert_seq;
    ghost_obj->S4FIFO.insert_bucket =
        S4FIFO_feature_collector_record_insert_ghost(params->feature_collector,
                                                     params->ghost_insert_seq);
    params->ghost_insert_seq++;
  }
  S4FIFO_feature_collector_record_one_hit(params->feature_collector);
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

  // if update cache is false, we only check the fifo and main caches
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
  params->seen_in_ghost_not_promoted = false;
  cache_obj_t *obj = params->small_fifo->find(params->small_fifo, req, true);
  if (obj != NULL) {
    // objects inserted within the last `small_skip_ratio` fraction of the
    // small FIFO's current object count are in "probation" and
    // re-requesting them does not count as a hit; this filters out
    // immediate re-requests (e.g. from scans) that are not indicative of
    // real reuse
    int64_t probation_len =
        (int64_t)(params->small_skip_ratio *
                  params->small_fifo->get_n_obj(params->small_fifo));
    if (S4FIFO_small_fifo_age(params, obj) >= probation_len) {
      obj->S4FIFO.freq += 1;
    }
    if (S4FIFO_is_collecting(params)) {
      S4FIFO_feature_collector_record_hit_small(params->feature_collector,
                                                obj->S4FIFO.insert_seq,
                                                params->small_insert_seq);
    }
    return obj;
  }

  cache_obj_t *ghost_obj = NULL;
  if (params->ghost_fifo != NULL) {
    ghost_obj = params->ghost_fifo->find(params->ghost_fifo, req, false);
  }
  if (ghost_obj != NULL) {
    if (S4FIFO_is_collecting(params)) {
      S4FIFO_feature_collector_record_hit_ghost(
          params->feature_collector, ghost_obj->S4FIFO.insert_seq,
          ghost_obj->S4FIFO.insert_bucket, params->ghost_insert_seq);
      S4FIFO_feature_collector_record_ghost_removal(params->feature_collector);
    }
    if (params->ghost_to_main_threshold <= 0) {
      params->ghost_fifo->remove(params->ghost_fifo, req->obj_id);
      params->hit_on_ghost = true;
    } else {
      params->seen_in_ghost_not_promoted = true;
    }
    // else: leave the entry in the ghost FIFO (it may be hit again before
    // it naturally ages out) and fall through - the object is simply
    // re-admitted to the small FIFO by S4FIFO_insert, like any other miss.
  }

  obj = params->main_fifo->find(params->main_fifo, req, true);
  if (obj != NULL) {
    obj->S4FIFO.freq += 1;
    if (S4FIFO_is_collecting(params)) {
      S4FIFO_feature_collector_record_hit_main(params->feature_collector,
                                               obj->S4FIFO.insert_seq,
                                               params->main_insert_seq);
    }
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

  if (params->hit_on_ghost) {
    /* insert into main FIFO */
    params->hit_on_ghost = false;
    obj = main_fifo->insert(main_fifo, req);
    S4FIFO_track_main_insert(params, obj);
  } else {
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
      S4FIFO_track_main_insert(params, obj);
    } else {
      obj = small_fifo->insert(small_fifo, req);
      S4FIFO_track_small_insert(params, obj);
    }
  }

  obj->S4FIFO.freq = 0;

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

static void S4FIFO_evict_small(cache_t *cache, const request_t *req) {
  S4FIFO_params_t *params = (S4FIFO_params_t *)cache->eviction_params;
  cache_t *small_fifo = params->small_fifo;
  cache_t *ghost_fifo = params->ghost_fifo;
  cache_t *main_fifo = params->main_fifo;

  bool has_evicted = false;
  while (!has_evicted && small_fifo->get_occupied_byte(small_fifo) > 0) {
    cache_obj_t *obj_to_evict = small_fifo->to_evict(small_fifo, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    // need to copy the object before it is evicted
    copy_cache_obj_to_request(params->req_local, obj_to_evict);

    if (obj_to_evict->S4FIFO.freq >= params->move_to_main_threshold) {
      cache_obj_t *main_obj = main_fifo->insert(main_fifo, params->req_local);
      S4FIFO_track_main_insert(params, main_obj);
    } else {
      if (ghost_fifo != NULL) {
        S4FIFO_track_ghost_insert(params, ghost_fifo, params->req_local);
      }
      if (S4FIFO_is_collecting(params)) {
        S4FIFO_feature_collector_record_one_hit(params->feature_collector);
      }
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
    int freq = obj_to_evict->S4FIFO.freq;
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    if (freq >= 1) {
      // we need to evict first because the object to insert has the same obj_id
      main_fifo->remove(main_fifo, obj_to_evict->obj_id);
      obj_to_evict = NULL;

      cache_obj_t *new_obj = main_fifo->insert(main_fifo, params->req_local);
      // clock with 2-bit counter
      new_obj->S4FIFO.freq = MIN(freq, 3) - 1;

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
// ****      learned control plane ("auto-tune=1"), optional          ****
// ****                                                               ****
// ***********************************************************************

static void S4FIFO_apply_config_entry(cache_t *cache, S4FIFO_params_t *params,
                                      const S4FIFOConfigEntry *cfg);

/**
 * @brief advance the WARMUP -> FEATURE_COLLECT -> PREDICTING phase machine,
 * applying a prediction and (if `prediction_interval` > 0) looping back to
 * FEATURE_COLLECT as needed. Called on every request; each phase does
 * nothing until its own condition is met, so this is cheap.
 */
static void S4FIFO_update_phase(cache_t *cache, S4FIFO_params_t *params) {
  int64_t n_req = cache->n_req;

  if (params->phase == S4FIFO_PHASE_WARMUP) {
    if (cache->get_occupied_byte(cache) >= cache->cache_size) {
      params->phase = S4FIFO_PHASE_FEATURE_COLLECT;
      params->phase_start_n_req = n_req;
    }
    return;
  }

  if (params->phase == S4FIFO_PHASE_FEATURE_COLLECT) {
    if (n_req - params->phase_start_n_req < params->feature_collect_reqs) {
      return;
    }
    S4FIFO_apply_predicted_config(cache, params);
    params->phase = S4FIFO_PHASE_PREDICTING;
    params->phase_start_n_req = n_req;
    return;
  }

  // S4FIFO_PHASE_PREDICTING: prediction_interval <= 0 means "predict once
  // and keep this configuration for the rest of the trace"
  if (params->prediction_interval <= 0) {
    return;
  }
  if (n_req - params->phase_start_n_req < params->prediction_interval) {
    return;
  }
  S4FIFO_feature_collector_init(
      params->feature_collector, cache->cache_size,
      params->small_fifo->cache_size, params->main_fifo->cache_size,
      params->ghost_fifo ? params->ghost_fifo->cache_size : 0,
      S4FIFO_FEATURE_DEFAULT_BUCKETS, params->feature_collect_reqs / 2);
  params->phase = S4FIFO_PHASE_FEATURE_COLLECT;
  params->phase_start_n_req = n_req;
}

/**
 * @brief snapshot the currently-collected features, predict a
 * configuration (see S4FIFO_predictor.h), and if the model was confident
 * enough to produce one, apply it: the two threshold/skip knobs take
 * effect immediately (they're read directly on every request), and the
 * two size-ratio knobs are applied "lazily" - the sub-FIFOs' cache_size
 * fields are updated to their new targets and the existing over-budget
 * eviction logic in S4FIFO_evict_small/_main drains (or grows into) each
 * one over subsequent requests, with no pause and no object migration.
 * Each sub-FIFO's hashtable was already sized for the parent's full
 * capacity at creation, so growing one is safe.
 */
static void S4FIFO_apply_predicted_config(cache_t *cache,
                                          S4FIFO_params_t *params) {
  S4FIFO_feature_vector_t fv;
  S4FIFO_feature_collector_get_features(params->feature_collector, &fv);

  if (params->dump_features) {
    double features76[76];
    if (s4fifo_dump_features76(&fv, features76)) {
      fprintf(stderr, "S4FIFO_FEATURES76=[");
      for (int i = 0; i < 76; i++) {
        fprintf(stderr, "%s%.17g", (i == 0) ? "" : ",", features76[i]);
      }
      fprintf(stderr, "]\n");
    }
  }

  S4FIFOConfigEntry cfg;
  if (!s4fifo_predict_auto(&fv, params->model_path, &cfg)) {
    INFO(
        "%s: not enough data collected (requests=%lld, hits=%lld), "
        "keeping current configuration\n",
        cache->cache_name, (long long)fv.total_requests,
        (long long)fv.total_hits);
    return;
  }

  INFO(
      "%s: applying learned configuration small-size-ratio=%.4lf,"
      "ghost-size-ratio=%.4lf,move-to-main-threshold=%d,"
      "ghost-to-main-threshold=%d,small-skip-ratio=%.4lf\n",
      cache->cache_name, cfg.small_size_ratio, cfg.ghost_size_ratio,
      cfg.move_to_main_threshold, cfg.ghost_to_main_threshold,
      cfg.small_skip_ratio);

  S4FIFO_apply_config_entry(cache, params, &cfg);
}

/**
 * @brief apply a single predicted config entry's 5 parameters to the
 * running cache (see S4FIFO_apply_predicted_config).
 */
static void S4FIFO_apply_config_entry(cache_t *cache, S4FIFO_params_t *params,
                                      const S4FIFOConfigEntry *cfg) {
  params->move_to_main_threshold = cfg->move_to_main_threshold;
  params->ghost_to_main_threshold = cfg->ghost_to_main_threshold;
  params->small_skip_ratio = cfg->small_skip_ratio;
  params->small_size_ratio = cfg->small_size_ratio;
  params->ghost_size_ratio = cfg->ghost_size_ratio;

  int64_t small_size = (int64_t)(cache->cache_size * cfg->small_size_ratio);
  int64_t main_size = cache->cache_size - small_size;
  int64_t ghost_size = (int64_t)(cache->cache_size * cfg->ghost_size_ratio);

  if (small_size > 0 && main_size > 0) {
    params->small_fifo->cache_size = small_size;
    params->main_fifo->cache_size = main_size;
  }
  if (params->ghost_fifo != NULL && ghost_size > 0) {
    params->ghost_fifo->cache_size = ghost_size;
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *S4FIFO_current_params(S4FIFO_params_t *params) {
  static __thread char params_str[512];
  snprintf(params_str, sizeof(params_str),
           "small-size-ratio=%.4lf,ghost-size-ratio=%.4lf,move-to-main-"
           "threshold=%d,ghost-to-main-threshold=%d,small-skip-ratio=%.4lf,"
           "auto-tune=%d,feature-collect-reqs=%lld,prediction-interval=%lld,"
           "model-path=%s\n",
           params->small_size_ratio, params->ghost_size_ratio,
           params->move_to_main_threshold, params->ghost_to_main_threshold,
           params->small_skip_ratio, params->auto_tune,
           (long long)params->feature_collect_reqs,
           (long long)params->prediction_interval, params->model_path);
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

    if (strcasecmp(key, "fifo-size-ratio") == 0 ||
        strcasecmp(key, "small-size-ratio") == 0) {
      params->small_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "ghost-to-main-threshold") == 0) {
      params->ghost_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "small-skip-ratio") == 0) {
      params->small_skip_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "auto-tune") == 0) {
      params->auto_tune = (atoi(value) != 0);
    } else if (strcasecmp(key, "feature-collect-reqs") == 0) {
      // either an absolute request count (>= 1) or a fraction of the trace
      // (0 < v < 1), which needs common_cache_params_t.n_total_req
      double v = strtod(value, NULL);
      if (v <= 0 || v >= 1) {
        params->feature_collect_reqs = (int64_t)v;
      } else if (params->n_total_req > 0) {
        params->feature_collect_reqs =
            (int64_t)(v * (double)params->n_total_req);
      } else {
        params->feature_collect_reqs = S4FIFO_FEATURE_COLLECT_REQS_FALLBACK;
        WARN_ONCE(
            "%s: feature-collect-reqs is a fraction but the trace length is "
            "unknown, collecting for %d requests instead - set "
            "common_cache_params_t.n_total_req (cachesim does this from the "
            "reader) or pass an absolute request count\n",
            cache->cache_name, S4FIFO_FEATURE_COLLECT_REQS_FALLBACK);
      }
    } else if (strcasecmp(key, "prediction-interval") == 0) {
      params->prediction_interval = strtoll(value, NULL, 10);
    } else if (strcasecmp(key, "model-path") == 0) {
      strncpy(params->model_path, value, sizeof(params->model_path) - 1);
      params->model_path[sizeof(params->model_path) - 1] = '\0';
    } else if (strcasecmp(key, "dump-features") == 0) {
      params->dump_features = (atoi(value) != 0);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", S4FIFO_current_params(params));
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

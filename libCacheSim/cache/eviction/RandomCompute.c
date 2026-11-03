//
//  RandomCompute.c
//  libCacheSim
//
//  Samples 128 objects at random and evicts the one with the lowest
//  cost / recency, so a block that is cheap to recompute and long unused goes
//  first.
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "dataStructure/hashtable/hashtable.h"
#include "float.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/macro.h"
#include "math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RANDOM_COMPUTE_N_SAMPLE 128

typedef struct {
  /// cache->n_req once the request being served has been replayed, or -1 if no
  /// simulator has reported a request (see RandomCompute_record_request)
  int64_t request_end_vtime;
} RandomCompute_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void RandomCompute_free(cache_t *cache);
static bool RandomCompute_get(cache_t *cache, const request_t *req);
static cache_obj_t *RandomCompute_find(cache_t *cache, const request_t *req,
                                       bool update_cache);
static cache_obj_t *RandomCompute_insert(cache_t *cache, const request_t *req);
static cache_obj_t *RandomCompute_to_evict(cache_t *cache,
                                           const request_t *req);
static void RandomCompute_evict(cache_t *cache, const request_t *req);
static bool RandomCompute_remove(cache_t *cache, obj_id_t obj_id);
static void RandomCompute_record_request(cache_t *cache, const obj_id_t *ids,
                                         int64_t n);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a RandomCompute cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params RandomCompute specific parameters, should be
 * NULL
 */
cache_t *RandomCompute_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache = cache_struct_init("RandomCompute", ccache_params_copy,
                                     cache_specific_params);
  cache->cache_init = RandomCompute_init;
  cache->cache_free = RandomCompute_free;
  cache->get = RandomCompute_get;
  cache->find = RandomCompute_find;
  cache->insert = RandomCompute_insert;
  cache->to_evict = RandomCompute_to_evict;
  cache->evict = RandomCompute_evict;
  cache->remove = RandomCompute_remove;
  cache->record_request = RandomCompute_record_request;

  RandomCompute_params_t *params =
      (RandomCompute_params_t *)malloc(sizeof(RandomCompute_params_t));
  params->request_end_vtime = -1;
  cache->eviction_params = params;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void RandomCompute_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief the time a block accessed now is stamped with
 *
 * Normally cache->n_req. While a simulator replays a multi-block request (see
 * RandomCompute_record_request), it is the time the replay ends instead, so
 * every block of the request gets the same last-access time. The request uses
 * them all at once; the order the simulator happens to replay them in is not
 * an age. Stamping each with its own n_req would make a block p positions
 * from the end of the replay look p accesses old. With a cost that grows with
 * position, as prefill cost does, that cancels the cost term for the deep
 * blocks of a long request: cost / recency tends to a constant there, and the
 * expensive blocks lose the protection the score is meant to give them.
 */
static inline int64_t _rc_now(const cache_t *cache) {
  const RandomCompute_params_t *params =
      (const RandomCompute_params_t *)cache->eviction_params;
  return params->request_end_vtime > cache->n_req ? params->request_end_vtime
                                                  : cache->n_req;
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
static bool RandomCompute_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief check whether an object is in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return true on hit, false on miss
 */
static cache_obj_t *RandomCompute_find(cache_t *cache, const request_t *req,
                                       bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj != NULL && update_cache) {
    obj->Random.last_access_vtime = _rc_now(cache);
  }

  return obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *RandomCompute_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);

  obj->Random.last_access_vtime = _rc_now(cache);

  return obj;
}

/**
 * @brief compute the cost function for RandomCompute eviction
 * cost = object_cost / max(1, time_since_last_access)
 *
 * Frequency used to be a factor here (cost * (freq + 1) / recency), but it
 * did not carry signal on the LLM traces: block reuse is driven by whether a
 * prefix is still live, which recency already captures, so the freq term only
 * added noise. Dropping it also makes the score independent of misc.freq,
 * which the core maintains for other purposes.
 */
static inline double _rc_cost(cache_t *cache, cache_obj_t *obj) {
  int64_t time_since_last_access = cache->n_req - obj->Random.last_access_vtime;
  int64_t recency = time_since_last_access > 1 ? time_since_last_access : 1;
  return (double)obj->cost / (double)recency;
}

/**
 * @brief the lowest-scoring of RANDOM_COMPUTE_N_SAMPLE random objects
 *
 * A pinned sample is passed over (see cache_set_pin_predicate), unless every
 * sample is pinned: pinning is best-effort, so an eviction always frees
 * something. Without pins this is the plain sampling loop.
 */
static cache_obj_t *_rc_sample_victim(cache_t *cache) {
  cache_obj_t *victim = NULL, *pinned_victim = NULL;
  double min_cost = DBL_MAX, min_pinned_cost = DBL_MAX;

  for (int i = 0; i < RANDOM_COMPUTE_N_SAMPLE; i++) {
    cache_obj_t *obj = hashtable_rand_obj(cache->hashtable);
    double cost = _rc_cost(cache, obj);
    if (cache_obj_is_pinned(obj)) {
      if (cost < min_pinned_cost) {
        min_pinned_cost = cost;
        pinned_victim = obj;
      }
    } else if (cost < min_cost) {
      min_cost = cost;
      victim = obj;
    }
  }

  return victim != NULL ? victim : pinned_victim;
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
static cache_obj_t *RandomCompute_to_evict(cache_t *cache,
                                           const request_t *req) {
  return _rc_sample_victim(cache);
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void RandomCompute_evict(cache_t *cache, const request_t *req) {
  cache_evict_base(cache, _rc_sample_victim(cache), true);
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
static bool RandomCompute_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  cache_remove_obj_base(cache, obj, true);

  return true;
}

/**
 * @brief a simulator is about to replay a request of n blocks, one access each
 *
 * Called between making room for the request and replaying it, when the clock
 * still reads the request's start; the replay advances it by n. See _rc_now.
 */
static void RandomCompute_record_request(cache_t *cache, const obj_id_t *ids,
                                         int64_t n) {
  (void)ids;
  RandomCompute_params_t *params =
      (RandomCompute_params_t *)cache->eviction_params;
  params->request_end_vtime = cache->n_req + n;
}

#ifdef __cplusplus
}
#endif

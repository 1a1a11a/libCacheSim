//
//  RandomTwo.c
//  libCacheSim
//
//  Picks two objects at random and evicts the one that is the least recently
//  used RandomTwo eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void RandomTwo_free(cache_t *cache);
static bool RandomTwo_get(cache_t *cache, const request_t *req);
static cache_obj_t *RandomTwo_find(cache_t *cache, const request_t *req,
                                   const bool update_cache);
static cache_obj_t *RandomTwo_insert(cache_t *cache, const request_t *req);
static cache_obj_t *RandomTwo_to_evict(cache_t *cache, const request_t *req);
static void RandomTwo_evict(cache_t *cache, const request_t *req);
static bool RandomTwo_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a RandomTwo cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params RandomTwo specific parameters, should be NULL
 */
cache_t *RandomTwo_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache =
      cache_struct_init("RandomTwo", ccache_params_copy, cache_specific_params);
  cache->cache_init = RandomTwo_init;
  cache->cache_free = RandomTwo_free;
  cache->get = RandomTwo_get;
  cache->find = RandomTwo_find;
  cache->insert = RandomTwo_insert;
  cache->to_evict = RandomTwo_to_evict;
  cache->evict = RandomTwo_evict;
  cache->remove = RandomTwo_remove;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void RandomTwo_free(cache_t *cache) { cache_struct_free(cache); }

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
static bool RandomTwo_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *RandomTwo_find(cache_t *cache, const request_t *req,
                                   const bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != NULL && update_cache) {
    obj->Random.last_access_vtime = cache->n_req;
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
static cache_obj_t *RandomTwo_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Random.last_access_vtime = cache->n_req;

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
static cache_obj_t *RandomTwo_to_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict1 = hashtable_rand_obj(cache->hashtable);
  cache_obj_t *obj_to_evict2 = hashtable_rand_obj(cache->hashtable);
  if (obj_to_evict1->Random.last_access_vtime <
      obj_to_evict2->Random.last_access_vtime)
    return obj_to_evict1;
  else
    return obj_to_evict2;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void RandomTwo_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict1 = hashtable_rand_obj(cache->hashtable);
  cache_obj_t *obj_to_evict2 = hashtable_rand_obj(cache->hashtable);
  if (obj_to_evict1->Random.last_access_vtime <
      obj_to_evict2->Random.last_access_vtime)
    cache_evict_base(cache, obj_to_evict1, true);
  else
    cache_evict_base(cache, obj_to_evict2, true);
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
static bool RandomTwo_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  cache_remove_obj_base(cache, obj, true);

  return true;
}

#ifdef __cplusplus
}
#endif

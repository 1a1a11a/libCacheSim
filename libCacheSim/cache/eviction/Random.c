//
//  Random.c
//  libCacheSim
//
//  Random eviction
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

static void Random_free(cache_t *cache);
static bool Random_get(cache_t *cache, const request_t *req);
static cache_obj_t *Random_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *Random_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Random_to_evict(cache_t *cache, const request_t *req);
static void Random_evict(cache_t *cache, const request_t *req);
static bool Random_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a Random cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params Random specific parameters, should be NULL
 */
cache_t *Random_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache =
      cache_struct_init("Random", ccache_params_copy, cache_specific_params);
  cache->cache_init = Random_init;
  cache->cache_free = Random_free;
  cache->get = Random_get;
  cache->find = Random_find;
  cache->insert = Random_insert;
  cache->to_evict = Random_to_evict;
  cache->evict = Random_evict;
  cache->remove = Random_remove;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Random_free(cache_t *cache) { cache_struct_free(cache); }

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
static bool Random_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *Random_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  return cache_find_base(cache, req, update_cache);
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
static cache_obj_t *Random_insert(cache_t *cache, const request_t *req) {
  return cache_insert_base(cache, req);
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
static cache_obj_t *Random_to_evict(cache_t *cache, const request_t *req) {
  return hashtable_rand_obj(cache->hashtable);
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void Random_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict = Random_to_evict(cache, req);
  DEBUG_ASSERT(obj_to_evict->obj_size != 0);
  cache_evict_base(cache, obj_to_evict, true);
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
static bool Random_remove(cache_t *cache, const obj_id_t obj_id) {
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

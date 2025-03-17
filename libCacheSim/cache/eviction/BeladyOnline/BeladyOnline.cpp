//  the online version of Belady eviction algorithm (MIN)
//  the implementation is inspired by the paper "Jain A, Lin C. Back to the future: Leveraging Belady's algorithm for improved cache replacement[J]. ACM SIGARCH Computer Architecture News, 2016, 44(3): 78-89."
//
//  BeladyOnline.c
//  libCacheSim
//
//
// Created by Xiaojun Guo on 3/27/25.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"
#include "./BeladyOnline_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define EVICT_IMMEDIATELY_IF_NO_FUTURE_ACCESS 1

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void BeladyOnline_free(cache_t *cache);
static bool BeladyOnline_get(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyOnline_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *BeladyOnline_insert(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyOnline_to_evict(cache_t *cache, const request_t *req);
static void BeladyOnline_evict(cache_t *cache, const request_t *req);
static bool BeladyOnline_remove(cache_t *cache, const obj_id_t obj_id);
static void BeladyOnline_remove_obj(cache_t *cache, cache_obj_t *obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a BeladyOnline cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params BeladyOnline specific parameters, should be NULL
 */
cache_t *BeladyOnline_init(const common_cache_params_t ccache_params,
                     __attribute__((unused))
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("BeladyOnline", ccache_params, cache_specific_params);
  cache->cache_init = BeladyOnline_init;
  cache->cache_free = BeladyOnline_free;
  cache->get = BeladyOnline_get;
  cache->find = BeladyOnline_find;
  cache->insert = BeladyOnline_insert;
  cache->evict = BeladyOnline_evict;
  cache->to_evict = BeladyOnline_to_evict;
  cache->remove = BeladyOnline_remove;

  BeladyOnline_t *params = BeladyOnline_new(ccache_params.cache_size);
  cache->eviction_params = params;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void BeladyOnline_free(cache_t *cache) {
  BeladyOnline_t *params = (BeladyOnline_t *)cache->eviction_params;
  BeladyOnline_delete(params);

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
 * For BeladyOnline, it performs different logic from other eviction algorithms. 
 * BealdyOnline uses a segment tree to record the occupancy of each time slot,
 * and when the object is accessed for the second time, BealdyOnline determines 
 * whether it was previously in the cache.
 * 
 * ```
 * for obj in trace:
 *   if(max(occ_vec[obj.last_access_vtime, current_vtime]) + obj.size <= cache_size):
 *     the object should be inserted into the cache at its last access_vtime
 *     occ_vec[obj.last_access_vtime, current_vtime] += obj.size
 *     return true
 *   else:
 *     return false
 * ```
 * 
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */
static bool BeladyOnline_get(cache_t *cache, const request_t *req) {
  BeladyOnline_t *params = (BeladyOnline_t *)cache->eviction_params;

  return _BeladyOnline_get(params, req->obj_id, req->obj_size + cache->obj_md_size);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief find an object in the cache
 *
 * BeladyOnline doesn't support this function
 * 
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *BeladyOnline_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  assert(false);

  return NULL;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * BeladyOnline doesn't support this function
 * 
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *BeladyOnline_insert(cache_t *cache, const request_t *req) {
  assert(false);

  return NULL;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * BeladyOnline doesn't support this function
 * 
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *BeladyOnline_to_evict(cache_t *cache, __attribute__((unused))
                                                    const request_t *req) {
  assert(false);
  return NULL;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * BeladyOnline doesn't support this function
 * 
 * @param cache
 * @param req not used
 */
static void BeladyOnline_evict(cache_t *cache,
                         __attribute__((unused)) const request_t *req) {
  assert(false);
}


/**
 * BeladyOnline doesn't support this function
 */
static void BeladyOnline_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(false);
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 *  BeladyOnline doesn't support this function
 * 
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool BeladyOnline_remove(cache_t *cache, const obj_id_t obj_id) {
  assert(false);
  return false;
}

#ifdef __cplusplus
}
#endif

//
//  MRU.c
//  libCacheSim
//
//  MRU eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef _cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} MRU_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void MRU_free(cache_t *cache);
static bool MRU_get(cache_t *cache, const request_t *req);
static cache_obj_t *MRU_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *MRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *MRU_to_evict(cache_t *cache, const request_t *req);
static void MRU_evict(cache_t *cache, const request_t *req);
static bool MRU_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *MRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params, cache_specific_params);
  cache->cache_init = MRU_init;
  cache->cache_free = MRU_free;
  cache->get = MRU_get;
  cache->find = MRU_find;
  cache->insert = MRU_insert;
  cache->evict = MRU_evict;
  cache->to_evict = MRU_to_evict;
  cache->remove = MRU_remove;

  cache->eviction_params = malloc(sizeof(MRU_params_t));
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void MRU_free(cache_t *cache) {
  free(cache->eviction_params);
  cache->eviction_params = NULL;
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
bool MRU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
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
static cache_obj_t *MRU_find(cache_t *cache, const request_t *req,
                      const bool update_cache) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj =
      cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  }
  return cache_obj;
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
static cache_obj_t *MRU_insert(cache_t *cache, const request_t *req) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

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
static cache_obj_t *MRU_to_evict(cache_t *cache, const request_t *req) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  return params->q_head;
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
static void MRU_evict(cache_t *cache, const request_t *req) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = MRU_to_evict(cache, req);
  params->q_head = params->q_head->queue.next;
  params->q_head->queue.prev = NULL;
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
static bool MRU_remove(cache_t *cache, const obj_id_t obj_id) {
  MRU_params_t *params = (MRU_params_t *)cache->eviction_params;
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);

  return true;
}

#ifdef _cplusplus
}
#endif

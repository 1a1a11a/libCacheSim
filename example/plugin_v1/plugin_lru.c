//
//  a plugin_lru module that supports different obj size
//
//
//  plugin_lru.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plugin_lru_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} plugin_lru_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void plugin_lru_free(cache_t *cache);
static bool plugin_lru_get(cache_t *cache, const request_t *req);
static cache_obj_t *plugin_lru_find(cache_t *cache, const request_t *req,
                                    bool update_cache);
static cache_obj_t *plugin_lru_insert(cache_t *cache, const request_t *req);
static cache_obj_t *plugin_lru_to_evict(cache_t *cache, const request_t *req);
static void plugin_lru_evict(cache_t *cache, const request_t *req);
static bool plugin_lru_remove(cache_t *cache, obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a plugin_lru cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params plugin_lru specific parameters, should be NULL
 * @return pointer to the initialized cache
 */
cache_t *plugin_lru_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("plugin_lru", ccache_params, cache_specific_params);
  cache->cache_init = plugin_lru_init;
  cache->cache_free = plugin_lru_free;
  cache->get = plugin_lru_get;
  cache->find = plugin_lru_find;
  cache->insert = plugin_lru_insert;
  cache->evict = plugin_lru_evict;
  cache->remove = plugin_lru_remove;

  plugin_lru_params_t *params = malloc(sizeof(plugin_lru_params_t));
  params->q_head = NULL;
  params->q_tail = NULL;
  cache->eviction_params = params;

  return cache;
}

/**
 * @brief free resources used by this cache
 *
 * @param cache the cache to free
 */
static void plugin_lru_free(cache_t *cache) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;
  free(params);
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
 * @param cache the cache
 * @param req the request
 * @return true if cache hit, false if cache miss
 */
static bool plugin_lru_get(cache_t *cache, const request_t *req) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache->find(cache, req, true);
  bool hit = (obj != NULL);

  if (hit) {
    move_obj_to_head(&params->q_head, &params->q_tail, obj);
  } else {
    while (cache->get_occupied_byte(cache) + req->obj_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }
    cache->insert(cache, req);
  }

  return hit;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief check whether an object is in the cache
 *
 * @param cache the cache
 * @param req the request
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the cache object if found, NULL otherwise
 */
static cache_obj_t *plugin_lru_find(cache_t *cache, const request_t *req,
                                    const bool update_cache) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  }
  return cache_obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache the cache
 * @param req the request
 * @return the inserted object
 */
static cache_obj_t *plugin_lru_insert(cache_t *cache, const request_t *req) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);

  return cache_obj;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 *
 * @param cache the cache
 * @param req the request (not used in LRU)
 * @return the object to be evicted (least recently used)
 */
static cache_obj_t *plugin_lru_to_evict(cache_t *cache, const request_t *req) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;
  return params->q_tail;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache the cache
 * @param req the request (not used)
 */
static void plugin_lru_evict(cache_t *cache, const request_t *req) {
  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);

  cache_remove_obj_base(cache, obj_to_evict, true);
}

/**
 * @brief remove the given object from the cache
 * note that eviction should not call this function, but rather call
 * `cache_evict_base` because we track extra metadata during eviction
 *
 * this function is different from eviction because it is used for user
 * triggered remove, and eviction is used by the cache to make space for new
 * objects
 *
 * @param cache the cache
 * @param obj the object to remove
 */
static void plugin_lru_remove_obj(cache_t *cache, cache_obj_t *obj) {
  if (obj == NULL) {
    return;
  }

  plugin_lru_params_t *params = (plugin_lru_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

/**
 * @brief remove an object from the cache by object ID
 * this is different from cache_evict because it is used for user triggered
 * remove, and eviction is used by the cache to make space for new objects
 *
 * @param cache the cache
 * @param obj_id the object ID to remove
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool plugin_lru_remove(cache_t *cache, const obj_id_t obj_id) {
  request_t req = {.obj_id = obj_id, .obj_size = 0};
  cache_obj_t *obj = cache_find_base(cache, &req, false);

  if (obj == NULL) {
    return false;
  }
  plugin_lru_remove_obj(cache, obj);
  return true;
}

#ifdef __cplusplus
}
#endif

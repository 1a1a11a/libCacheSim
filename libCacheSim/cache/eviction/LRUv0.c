//
//  this is the version using glib hashtable
//  compared to LRU, this uses more memory and is slower
//
//
//  LRUv0.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include <assert.h>
#include <glib.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LRUv0_params {
  GHashTable *hashtable;
  GQueue *list;
} LRUv0_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LRUv0_parse_params(cache_t *cache,
                               const char *cache_specific_params);
static void LRUv0_free(cache_t *cache);
static bool LRUv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *LRUv0_find(cache_t *cache, const request_t *req,
                               const bool update_cache);
static cache_obj_t *LRUv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LRUv0_to_evict(cache_t *cache, const request_t *req);
static void LRUv0_evict(cache_t *cache, const request_t *req);
static bool LRUv0_remove(cache_t *cache, const obj_id_t obj_id);

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
cache_t *LRUv0_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRUv0", ccache_params, cache_specific_params);
  cache->cache_init = LRUv0_init;
  cache->cache_free = LRUv0_free;
  cache->get = LRUv0_get;
  cache->find = LRUv0_find;
  cache->insert = LRUv0_insert;
  cache->evict = LRUv0_evict;
  cache->remove = LRUv0_remove;
  cache->to_evict = LRUv0_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = g_new0(LRUv0_params_t, 1);
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  LRUv0_params->hashtable =
      g_hash_table_new_full(g_int64_hash, g_direct_equal, NULL, NULL);
  LRUv0_params->list = g_queue_new();
  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LRUv0_free(cache_t *cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  g_hash_table_destroy(LRUv0_params->hashtable);
  g_queue_free_full(LRUv0_params->list, (GDestroyNotify)free_cache_obj);
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
static bool LRUv0_get(cache_t *cache, const request_t *req) {
  bool cache_hit = LRUv0_find(cache, req, true);
  if (req->obj_size <= cache->cache_size) {
    if (!cache_hit) LRUv0_insert(cache, req);

    while (cache->occupied_byte > cache->cache_size)
      LRUv0_evict(cache, req);
  } else {
    WARN("req %lld: obj size %ld larger than cache size %ld\n",
         (long long)req->obj_id, (long)req->obj_size, (long)cache->cache_size);
  }
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
static cache_obj_t *LRUv0_find(cache_t *cache, const request_t *req,
                               const bool update_cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             GSIZE_TO_POINTER(req->obj_id));
  if (node == NULL) return NULL;

  // not the best way
  if (!update_cache) return node->data;

  cache_obj_t *cache_obj = node->data;
  if (likely(update_cache)) {
    if (unlikely(cache_obj->obj_size != req->obj_size)) {
      cache->occupied_byte -= cache_obj->obj_size;
      cache->occupied_byte += req->obj_size;
      cache_obj->obj_size = req->obj_size;
    }
  }
  g_queue_unlink(LRUv0_params->list, node);
  g_queue_push_tail_link(LRUv0_params->list, node);

  return node->data;
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
static cache_obj_t *LRUv0_insert(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);

  cache->occupied_byte += req->obj_size + cache->obj_md_size;
  cache_obj_t *cache_obj = create_cache_obj_from_request(req);
  // TODO: use SList should be more memory efficient than queue which uses
  // doubly-linkedlist under the hood
  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(LRUv0_params->list, node);
  g_hash_table_insert(LRUv0_params->hashtable,
                      GSIZE_TO_POINTER(cache_obj->obj_id), (gpointer)node);

  return cache_obj;
}

static cache_obj_t *LRUv0_get_cached_obj(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             GSIZE_TO_POINTER(req->obj_id));
  cache_obj_t *cache_obj = node->data;
  return cache_obj;
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
static cache_obj_t *LRUv0_to_evict(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  return (cache_obj_t *)g_queue_peek_head(LRUv0_params->list);
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
static void LRUv0_evict(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = LRUv0_to_evict(cache, req);
  assert(cache->occupied_byte >= cache_obj->obj_size);
  cache->occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
  g_hash_table_remove(LRUv0_params->hashtable,
                      (gconstpointer)cache_obj->obj_id);
  free_cache_obj(cache_obj);
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
static bool LRUv0_remove(cache_t *cache, const obj_id_t obj_id) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);

  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             (gconstpointer)obj_id);
  if (node == NULL) {
    return false;
  }

  cache_obj_t *cache_obj = (cache_obj_t *)(node->data);
  assert(cache->occupied_byte >= cache_obj->obj_size + cache->obj_md_size);
  cache->occupied_byte -=
      (((cache_obj_t *)(node->data))->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;

  g_queue_delete_link(LRUv0_params->list, (GList *)node);
  g_hash_table_remove(LRUv0_params->hashtable, (gconstpointer)obj_id);
  free_cache_obj(cache_obj);

  return true;
}

#ifdef __cplusplus
}
#endif
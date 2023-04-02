//
//  the Size eviction algorithm (MIN)
//
//
//  Size.c
//  libCacheSim
//
//
// Created by Juncheng Yang on 3/30/21.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../dataStructure/pqueue.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Size_params {
  /* a priority queue ordering the object size */
  pqueue_t *pq;
} Size_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Size_free(cache_t *cache);
static bool Size_get(cache_t *cache, const request_t *req);
static cache_obj_t *Size_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *Size_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Size_to_evict(cache_t *cache, const request_t *req);
static void Size_evict(cache_t *cache, const request_t *req);
static bool Size_remove(cache_t *cache, const obj_id_t obj_id);
static void Size_remove_obj(cache_t *cache, cache_obj_t *obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a Size cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params Size specific parameters, should be NULL
 */
cache_t *Size_init(const common_cache_params_t ccache_params,
                     __attribute__((unused))
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Size", ccache_params, cache_specific_params);
  cache->cache_init = Size_init;
  cache->cache_free = Size_free;
  cache->get = Size_get;
  cache->find = Size_find;
  cache->insert = Size_insert;
  cache->evict = Size_evict;
  cache->to_evict = Size_to_evict;
  cache->remove = Size_remove;

  Size_params_t *params = my_malloc(Size_params_t);
  cache->eviction_params = params;

  params->pq = pqueue_init((unsigned long)8e6);
  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Size_free(cache_t *cache) {
  Size_params_t *params = cache->eviction_params;
  pq_node_t *node = pqueue_pop(params->pq);
  while (node) {
    my_free(sizeof(pq_node_t), node);
    node = pqueue_pop(params->pq);
  }
  pqueue_free(params->pq);

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
static bool Size_get(cache_t *cache, const request_t *req) {
  Size_params_t *params = cache->eviction_params;
  DEBUG_ASSERT(cache->n_obj == params->pq->size - 1);
  bool ret = cache_get_base(cache, req);

  return ret;
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
static cache_obj_t *Size_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  Size_params_t *params = cache->eviction_params;
  cache_obj_t *cached_obj = cache_find_base(cache, req, update_cache);

  if (!update_cache) return cached_obj;

  if (cached_obj == NULL) {
    return NULL;
  }

  pqueue_pri_t pri = {.pri = req->obj_size};
  pqueue_change_priority(params->pq, pri,
                         (pq_node_t *)(cached_obj->Size.pq_node));
  return cached_obj;
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
static cache_obj_t *Size_insert(cache_t *cache, const request_t *req) {
  Size_params_t *params = cache->eviction_params;

  cache_obj_t *cached_obj = cache_insert_base(cache, req);

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id;
  node->pri.pri = req->obj_size;
  pqueue_insert(params->pq, (void *)node);
  cached_obj->Size.pq_node = node;

  return cached_obj;
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
static cache_obj_t *Size_to_evict(cache_t *cache, __attribute__((unused))
                                                    const request_t *req) {
  Size_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_peek(params->pq);
  return hashtable_find_obj_id(cache->hashtable, node->obj_id);
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void Size_evict(cache_t *cache,
                         __attribute__((unused)) const request_t *req) {
  Size_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_pop(params->pq);

  cache_obj_t *obj_to_evict =
      hashtable_find_obj_id(cache->hashtable, node->obj_id);
  DEBUG_ASSERT(node == obj_to_evict->Size.pq_node);

  obj_to_evict->Size.pq_node = NULL;
  my_free(sizeof(pq_node_t), node);

  cache_evict_base(cache, obj_to_evict, true);
}

static void Size_remove_obj(cache_t *cache, cache_obj_t *obj) {
  Size_params_t *params = cache->eviction_params;
  DEBUG_ASSERT(obj != NULL);

  if (obj->Size.pq_node != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj->Size.pq_node);
    my_free(sizeof(pq_node_t), obj->Size.pq_node);
    obj->Size.pq_node = NULL;
  }

  cache_remove_obj_base(cache, obj, true);
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
static bool Size_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  Size_remove_obj(cache, obj);
  return true;
}

#ifdef __cplusplus
}
#endif

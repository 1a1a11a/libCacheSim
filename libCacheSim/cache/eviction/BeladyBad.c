//
//  the Belady eviction algorithm with some modifications to explore its
//  performance
//
//
//  BeladyBad.c
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

typedef struct BeladyBad_params {
  /* a priority queue recording the next access time */
  pqueue_t *pq;
  int64_t n_eviction;
  // objects are kept in the cache till at least MIN_CACHE_STAY_TIME * cache
  // objects are inserted
  double min_cache_stay_time;

  int64_t n_obj_not_in_pq;
  cache_obj_t *first_obj_not_in_pq;
  cache_obj_t *last_obj_not_in_pq;
} BeladyBad_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void BeladyBad_free(cache_t *cache);
static bool BeladyBad_get(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyBad_find(cache_t *cache, const request_t *req,
                                   const bool update_cache);
static cache_obj_t *BeladyBad_insert(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyBad_to_evict(cache_t *cache, const request_t *req);
static void BeladyBad_evict(cache_t *cache, const request_t *req);
static bool BeladyBad_remove(cache_t *cache, const obj_id_t obj_id);
static void BeladyBad_remove_obj(cache_t *cache, cache_obj_t *obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a BeladyBad cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params BeladyBad specific parameters, should be NULL
 */
cache_t *BeladyBad_init(const common_cache_params_t ccache_params,
                        __attribute__((unused))
                        const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("BeladyBad", ccache_params);
  cache->cache_init = BeladyBad_init;
  cache->cache_free = BeladyBad_free;
  cache->get = BeladyBad_get;
  cache->find = BeladyBad_find;
  cache->insert = BeladyBad_insert;
  cache->evict = BeladyBad_evict;
  cache->to_evict = BeladyBad_to_evict;
  cache->remove = BeladyBad_remove;
  cache->init_params = cache_specific_params;

  if (cache_specific_params != NULL) {
    printf("BeladyBad does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  BeladyBad_params_t *params = my_malloc(BeladyBad_params_t);
  cache->eviction_params = params;
  memset(params, 0, sizeof(BeladyBad_params_t));
  params->min_cache_stay_time = 0.2;

  params->pq = pqueue_init((unsigned long)8e6);
  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void BeladyBad_free(cache_t *cache) {
  BeladyBad_params_t *params = cache->eviction_params;
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
static bool BeladyBad_get(cache_t *cache, const request_t *req) {
  /* -2 means the trace does not have next_access ts information */
  DEBUG_ASSERT(req->next_access_vtime != -2);
  BeladyBad_params_t *params = cache->eviction_params;

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
static cache_obj_t *BeladyBad_find(cache_t *cache, const request_t *req,
                                   const bool update_cache) {
  BeladyBad_params_t *params = cache->eviction_params;
  cache_obj_t *cached_obj = cache_find_base(cache, req, update_cache);

  if (!update_cache) return cached_obj;

  if (cached_obj == NULL) {
    return NULL;
  }

  cached_obj->misc.next_access_vtime = req->next_access_vtime;
  pqueue_pri_t pri = {.pri = req->next_access_vtime};
  if (cached_obj->queue.next == NULL && cached_obj->queue.prev == NULL) {
    // in the priority queue
    pqueue_change_priority(params->pq, pri,
                           (pq_node_t *)(cached_obj->Belady.pq_node));
  } else {
    pq_node_t *node = cached_obj->Belady.pq_node;
    assert(node->obj_id == req->obj_id);
    node->pri.pri = req->next_access_vtime;
  }

#ifdef TRACK_EVICTION_V_AGE_SINCE_LAST_REQUEST
  if (req->next_access_vtime == INT64_MAX) {
    BeladyBad_evict(cache, req);
  }
#endif

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
static cache_obj_t *BeladyBad_insert(cache_t *cache, const request_t *req) {
  BeladyBad_params_t *params = cache->eviction_params;

  if (req->next_access_vtime == -1) {
    ERROR("next access time is -1, please use INT64_MAX instead\n");
  }

  cache_obj_t *cached_obj = cache_insert_base(cache, req);

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id;
  node->pri.pri = req->next_access_vtime;
  cached_obj->Belady.pq_node = node;
  cached_obj->misc.next_access_vtime = req->next_access_vtime;
  append_obj_to_tail(&params->first_obj_not_in_pq, &params->last_obj_not_in_pq,
                     cached_obj);

  params->n_obj_not_in_pq++;

  if (params->n_obj_not_in_pq > params->min_cache_stay_time * cache->n_obj) {
    cache_obj_t *obj = params->first_obj_not_in_pq;
    remove_obj_from_list(&params->first_obj_not_in_pq,
                         &params->last_obj_not_in_pq, obj);
    params->n_obj_not_in_pq--;
    obj->queue.next = NULL;
    obj->queue.prev = NULL;
    pqueue_insert(params->pq, (void *)obj->Belady.pq_node);
  }

#ifdef TRACK_EVICTION_V_AGE_SINCE_LAST_REQUEST
  if (req->next_access_vtime == INT64_MAX) {
    BeladyBad_evict(cache, req);
  }
#endif

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
static cache_obj_t *BeladyBad_to_evict(cache_t *cache, __attribute__((unused))
                                                       const request_t *req) {
  BeladyBad_params_t *params = cache->eviction_params;
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
static void BeladyBad_evict(cache_t *cache,
                            __attribute__((unused)) const request_t *req) {
  BeladyBad_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_pop(params->pq);

  cache_obj_t *obj_to_evict =
      hashtable_find_obj_id(cache->hashtable, node->obj_id);
  DEBUG_ASSERT(node == obj_to_evict->Belady.pq_node);

  obj_to_evict->Belady.pq_node = NULL;
  my_free(sizeof(pq_node_t), node);

  cache_evict_base(cache, obj_to_evict, true);
}

static void BeladyBad_remove_obj(cache_t *cache, cache_obj_t *obj) {
  BeladyBad_params_t *params = cache->eviction_params;
  DEBUG_ASSERT(obj != NULL);

  if (obj->Belady.pq_node != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj->Belady.pq_node);
    my_free(sizeof(pq_node_t), obj->Belady.pq_node);
    obj->Belady.pq_node = NULL;
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
static bool BeladyBad_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  BeladyBad_remove_obj(cache, obj);
  return true;
}

#ifdef __cplusplus
}
#endif

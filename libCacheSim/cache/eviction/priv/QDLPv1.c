//
//  Quick demotion + lazy promoition v1
//
//  20% FIFO + ARC
//  insert to ARC when evicting from FIFO
//
//
//  QDLPv1.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *fifo;
  cache_t *fifo_ghost;
  cache_t *main_cache;
  request_t *req_local;
  bool hit_on_ghost;
} QDLPv1_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

cache_t *QDLPv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);
static void QDLPv1_free(cache_t *cache);
static bool QDLPv1_get(cache_t *cache, const request_t *req);

static cache_obj_t *QDLPv1_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *QDLPv1_insert(cache_t *cache, const request_t *req);
static cache_obj_t *QDLPv1_to_evict(cache_t *cache, const request_t *req);
static void QDLPv1_evict(cache_t *cache, const request_t *req);
static bool QDLPv1_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t QDLPv1_get_occupied_byte(const cache_t *cache);
static inline int64_t QDLPv1_get_n_obj(const cache_t *cache);
static inline bool QDLPv1_can_insert(cache_t *cache, const request_t *req);


// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *QDLPv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("QDLPv1", ccache_params);
  cache->cache_init = QDLPv1_init;
  cache->cache_free = QDLPv1_free;
  cache->get = QDLPv1_get;
  cache->find = QDLPv1_find;
  cache->insert = QDLPv1_insert;
  cache->evict = QDLPv1_evict;
  cache->remove = QDLPv1_remove;
  cache->to_evict = QDLPv1_to_evict;
  cache->init_params = cache_specific_params;
  cache->get_occupied_byte = QDLPv1_get_occupied_byte;
  cache->get_n_obj = QDLPv1_get_n_obj;
  cache->can_insert = QDLPv1_can_insert;

  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(QDLPv1_params_t));
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  params->req_local = new_request();
  int64_t fifo_cache_size = (int64_t)ccache_params.cache_size * 0.20;
  int64_t main_cache_size = ccache_params.cache_size - fifo_cache_size;
  int64_t fifo_ghost_cache_size = main_cache_size;

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = fifo_cache_size;
  params->fifo = FIFO_init(ccache_params_local, NULL);

  ccache_params_local.cache_size = fifo_ghost_cache_size;
  params->fifo_ghost = FIFO_init(ccache_params_local, NULL);

  ccache_params_local.cache_size = main_cache_size;
  params->main_cache = ARC_init(ccache_params_local, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv1-%.2lf-ghost", 0.20);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void QDLPv1_free(cache_t *cache) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  free_request(params->req_local);
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
static bool QDLPv1_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *QDLPv1_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  params->hit_on_ghost = false;
  cache_obj_t *obj = params->fifo->find(params->fifo, req, false);

  if (obj != NULL) {
    if (update_cache) {
      obj->misc.freq = 1;
    }
    return obj;
  }

  cache_obj_t *obj_ghost =
      params->fifo_ghost->find(params->fifo_ghost, req, false);
  if (obj_ghost != NULL) {
    params->hit_on_ghost = true;
    params->fifo_ghost->remove(params->fifo_ghost, req->obj_id);
  }

  obj = params->main_cache->find(params->main_cache, req, false);
  bool main_cache_hit = obj != NULL;

  if (main_cache_hit && update_cache) {
    params->main_cache->find(params->main_cache, req, true);
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
static cache_obj_t *QDLPv1_insert(cache_t *cache, const request_t *req) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    obj = params->main_cache->insert(params->main_cache, req);
    obj->misc.q_id = 2;
  } else {
    /* insert into the fifo */
    obj = params->fifo->insert(params->main_cache, req);
    obj->misc.q_id = 1;
  }
  obj->misc.freq = 0;
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
static cache_obj_t *QDLPv1_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
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
static void QDLPv1_evict(cache_t *cache, const request_t *req) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;

  // evict from FIFO
  cache_obj_t *obj = params->fifo->to_evict(params->fifo, req);
  // need to copy the object before it is evicted
  copy_cache_obj_to_request(params->req_local, obj);

  if (obj->misc.freq > 0) {
    // promote to main cache
    // get will insert to and evict from main cache
    params->main_cache->get(params->main_cache, params->req_local);
    // remove from fifo1, but do not update stat
    params->fifo->evict(params->fifo, req);
  } else {
    // evict from fifo1 and insert to ghost
    params->fifo->evict(params->fifo, req);
    params->fifo_ghost->get(params->fifo_ghost, params->req_local);
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
static bool QDLPv1_remove(cache_t *cache, const obj_id_t obj_id) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->fifo->remove(params->fifo, obj_id);
  removed = removed || params->fifo_ghost->remove(params->fifo_ghost, obj_id);
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t QDLPv1_get_occupied_byte(const cache_t *cache) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  return params->fifo->get_occupied_byte(params->fifo) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t QDLPv1_get_n_obj(const cache_t *cache) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  return params->fifo->get_n_obj(params->fifo) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool QDLPv1_can_insert(cache_t *cache, const request_t *req) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;

  return req->obj_size <= params->fifo->cache_size;
}


#ifdef __cplusplus
}
#endif

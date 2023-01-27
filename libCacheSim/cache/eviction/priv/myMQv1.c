//
//  multi queue 
//  each queue has a specified size and a specified ghost size
//  promotion upon hit
//
//  20% FIFO + 80% 2-bit clock, promote when evicting from FIFO
//
//
//  myMQv1.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *fifo1_head;
  cache_obj_t *fifo1_tail;
  cache_obj_t *fifo2_head;
  cache_obj_t *fifo2_tail;

  int64_t n_fifo1_obj;
  int64_t n_fifo2_obj;
  int64_t n_fifo1_byte;
  int64_t n_fifo2_byte;

  int64_t fifo1_max_size;
  int64_t fifo2_max_size;

} myMQv1_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void myMQv1_parse_params(cache_t *cache, const char *cache_specific_params);
static void myMQv1_free(cache_t *cache);
static bool myMQv1_get(cache_t *cache, const request_t *req);
static cache_obj_t *myMQv1_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *myMQv1_insert(cache_t *cache, const request_t *req);
static cache_obj_t *myMQv1_to_evict(cache_t *cache, const request_t *req);
static void myMQv1_evict(cache_t *cache, const request_t *req);
static bool myMQv1_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void myMQv1_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *myMQv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("myMQv1", ccache_params);
  cache->cache_init = myMQv1_init;
  cache->cache_free = myMQv1_free;
  cache->get = myMQv1_get;
  cache->find = myMQv1_find;
  cache->insert = myMQv1_insert;
  cache->evict = myMQv1_evict;
  cache->remove = myMQv1_remove;
  cache->to_evict = myMQv1_to_evict;
  cache->init_params = cache_specific_params;
  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(myMQv1_params_t));
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
  params->fifo1_head = NULL;
  params->fifo1_tail = NULL;
  params->fifo2_head = NULL;
  params->fifo2_tail = NULL;
  params->n_fifo1_obj = 0;
  params->n_fifo2_obj = 0;
  params->n_fifo1_byte = 0;
  params->n_fifo2_byte = 0;
  params->fifo1_max_size = (int64_t)ccache_params.cache_size * 0.25;
  params->fifo2_max_size = ccache_params.cache_size - params->fifo1_max_size;

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "myMQv1-%.2lf", 0.25);
  return cache;
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
bool myMQv1_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *myMQv1_find(cache_t *cache, const request_t *req,
                 const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);
  if (cache_obj != NULL && update_cache) {
    if (cache_obj->misc.freq < 4) {
        cache_obj->misc.freq += 1;
    }
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
static cache_obj_t *myMQv1_insert(cache_t *cache, const request_t *req) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->fifo1_head, &params->fifo1_tail, obj);
  params->n_fifo1_obj += 1;
  params->n_fifo1_byte += req->obj_size + cache->obj_md_size;

  obj->misc.freq = 0;
  obj->misc.q_id = 1;

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
static cache_obj_t *myMQv1_to_evict(cache_t *cache, const request_t *req) {
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
static void myMQv1_evict(cache_t *cache, const request_t *req) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = params->fifo1_tail;
  params->fifo1_tail = obj->queue.prev;
  if (params->fifo1_tail != NULL) {
    params->fifo1_tail->queue.next = NULL;
  } else {
    params->fifo1_head = NULL;
  }
  DEBUG_ASSERT(obj->misc.q_id == 1);
  params->n_fifo1_obj -= 1;
  params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;

  if (obj->misc.freq > 0) {
    // promote to clock
    obj->misc.q_id = 2;
    obj->misc.freq -= 1;
    params->n_fifo2_obj += 1;
    params->n_fifo2_byte += obj->obj_size + cache->obj_md_size;
    prepend_obj_to_head(&params->fifo2_head, &params->fifo2_tail, obj);
    while (params->n_fifo2_byte > params->fifo2_max_size) {
      cache_obj_t *obj_to_evict = params->fifo2_tail;
      if (obj_to_evict->misc.freq > 0) {
        obj_to_evict->misc.freq -= 1;
        move_obj_to_head(&params->fifo2_head, &params->fifo2_tail,
                         obj_to_evict);
        continue;
      } else {
        remove_obj_from_list(&params->fifo2_head, &params->fifo2_tail,
                             obj_to_evict);
        params->n_fifo2_obj -= 1;
        params->n_fifo2_byte -= obj_to_evict->obj_size + cache->obj_md_size;
        cache_evict_base(cache, obj_to_evict, true);
      }
    }
  } else {
    // evict from fifo1
    cache_evict_base(cache, obj, true);
  }
}

static void myMQv1_remove_obj(cache_t *cache, cache_obj_t *obj) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  if (obj->misc.q_id == 1) {
    params->n_fifo1_obj -= 1;
    params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo1_head, &params->fifo1_tail, obj);
  } else if (obj->misc.q_id == 2) {
    params->n_fifo2_obj -= 1;
    params->n_fifo2_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo2_head, &params->fifo2_tail, obj);
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
static bool myMQv1_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  myMQv1_remove_obj(cache, obj);

  return true;
}

#ifdef __cplusplus
}
#endif

//
//  a LRU_Belady module that supports different obj size
//
//
//  LRU_Belady.c
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

// #define USE_BELADY_PROMOTION
// #define USE_BELADY_INSERTION
#define USE_BELADY_EVICTION

/* used by LFU related */
typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
  int64_t n_miss;
} LRU_Belady_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LRU_Belady_free(cache_t *cache);
static bool LRU_Belady_get(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_Belady_find(cache_t *cache, const request_t *req,
                                    const bool update_cache);
static cache_obj_t *LRU_Belady_insert(cache_t *cache, const request_t *req);
static void LRU_Belady_evict(cache_t *cache, const request_t *req);
static bool LRU_Belady_remove(cache_t *cache, const obj_id_t obj_id);
static void LRU_Belady_print_cache(const cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a LRU_Belady cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params LRU_Belady specific parameters, should be NULL
 */
cache_t *LRU_Belady_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LRU_Belady", ccache_params, cache_specific_params);
  cache->cache_init = LRU_Belady_init;
  cache->cache_free = LRU_Belady_free;
  cache->get = LRU_Belady_get;
  cache->find = LRU_Belady_find;
  cache->insert = LRU_Belady_insert;
  cache->evict = LRU_Belady_evict;
  cache->remove = LRU_Belady_remove;
  cache->to_evict = NULL;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->print_cache = LRU_Belady_print_cache;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

#ifdef USE_BELADY_INSERTION
  snprintf(cache->cache_name + strlen(cache->cache_name), CACHE_NAME_ARRAY_LEN,
           "%s", "_in");
#endif
#ifdef USE_BELADY_PROMOTION
  snprintf(cache->cache_name + strlen(cache->cache_name), CACHE_NAME_ARRAY_LEN,
           "%s", "_po");
#endif
#ifdef USE_BELADY_EVICTION
  snprintf(cache->cache_name + strlen(cache->cache_name), CACHE_NAME_ARRAY_LEN,
           "%s", "_ev");
#endif

  LRU_Belady_params_t *params = malloc(sizeof(LRU_Belady_params_t));
  params->q_head = NULL;
  params->q_tail = NULL;
  params->n_miss = 0;
  cache->eviction_params = params;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LRU_Belady_free(cache_t *cache) { cache_struct_free(cache); }

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
static bool LRU_Belady_get(cache_t *cache, const request_t *req) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  bool ck = cache_get_base(cache, req);
  if (!ck) params->n_miss++;

  return ck;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static bool should_insert(cache_t *cache, int64_t next_access_vtime) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  double miss_ratio = (double)(params->n_miss) / (double)(cache->n_req);
  int64_t n_req_thresh = (int64_t)(double)(cache->cache_size) / miss_ratio;
  // printf("%.4lf %ld\n", miss_ratio, n_req_thresh);
  // printf("%ld %ld\n", next_access_vtime - cache->n_req, n_req_thresh);
  if (next_access_vtime - cache->n_req <= n_req_thresh) {
    return true;
  } else {
    return false;
  }
}

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
static cache_obj_t *LRU_Belady_find(cache_t *cache, const request_t *req,
                                    const bool update_cache) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    /* LRU_Belady_head is the newest, move cur obj to LRU_Belady_head */
#ifdef USE_BELADY_PROMOTION
    if (should_insert(cache, req->next_access_vtime)) {
      move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
    }
#else
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
#endif
  }
  return cache_obj;
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
static cache_obj_t *LRU_Belady_insert(cache_t *cache, const request_t *req) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;

  cache_obj_t *obj = NULL;

#ifdef USE_BELADY_INSERTION
  if (should_insert(cache, req->next_access_vtime)) {
    obj = cache_insert_base(cache, req);
    prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  }
#else
  obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
#endif

  return obj;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void LRU_Belady_evict(cache_t *cache, const request_t *req) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

#ifdef USE_BELADY_EVICTION
  if (should_insert(cache, obj_to_evict->misc.next_access_vtime)) {
    move_obj_to_head(&params->q_head, &params->q_tail, obj_to_evict);
  } else {
    remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
    cache_evict_base(cache, obj_to_evict, true);
  }
#else

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);

  cache_evict_base(cache, obj_to_evict, true);
#endif
}

/**
 * @brief remove the given object from the cache
 * note that eviction should not call this function, but rather call
 * `cache_evict_base` because we track extra metadata during eviction
 *
 * and this function is different from eviction
 * because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj
 */
static void LRU_Belady_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);

  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
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
static bool LRU_Belady_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);

  return true;
}

static void LRU_Belady_print_cache(const cache_t *cache) {
  LRU_Belady_params_t *params = (LRU_Belady_params_t *)cache->eviction_params;
  cache_obj_t *cur = params->q_head;
  // print from the most recent to the least recent
  if (cur == NULL) {
    printf("empty\n");
    return;
  }
  while (cur != NULL) {
    printf("%ld->", (long)cur->obj_id);
    cur = cur->queue.next;
  }
  printf("END\n");
}

#ifdef __cplusplus
}
#endif

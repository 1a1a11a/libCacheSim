//
//  LRU with probabilistic promotion
//
//
//  LRU_Prob.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"
#include "../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LRU_Prob_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  double prob;
  int threshold;
} LRU_Prob_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LRU_Prob_parse_params(cache_t *cache, const char *cache_specific_params);
static void LRU_Prob_free(cache_t *cache);
static bool LRU_Prob_get(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_Prob_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *LRU_Prob_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_Prob_to_evict(cache_t *cache, const request_t *req);
static void LRU_Prob_evict(cache_t *cache, const request_t *req);
static bool LRU_Prob_remove(cache_t *cache, const obj_id_t obj_id);

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
cache_t *LRU_Prob_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRU_Prob", ccache_params, cache_specific_params);
  cache->cache_init = LRU_Prob_init;
  cache->cache_free = LRU_Prob_free;
  cache->get = LRU_Prob_get;
  cache->find = LRU_Prob_find;
  cache->insert = LRU_Prob_insert;
  cache->evict = LRU_Prob_evict;
  cache->remove = LRU_Prob_remove;
  cache->to_evict = LRU_Prob_to_evict;
  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params =
      (LRU_Prob_params_t *)malloc(sizeof(LRU_Prob_params_t));
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)(cache->eviction_params);
  params->prob = 0.5;

  if (cache_specific_params != NULL) {
    LRU_Prob_parse_params(cache, cache_specific_params);
  }

  params->threshold = (int)1.0 / params->prob;
  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "LRU_Prob_%lf",
           params->prob);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LRU_Prob_free(cache_t *cache) {
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
static bool LRU_Prob_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t* LRU_Prob_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;

  cache_obj_t *cached_obj = cache_find_base(cache, req, update_cache);
  if (cached_obj != NULL && likely(update_cache)) {
    cached_obj->misc.freq += 1;
    cached_obj->misc.next_access_vtime = req->next_access_vtime;

    if (next_rand() % params->threshold == 0) {
      move_obj_to_head(&params->q_head, &params->q_tail, cached_obj);
    }
  }

  return cached_obj;
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
static cache_obj_t *LRU_Prob_insert(cache_t *cache, const request_t *req) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
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
static cache_obj_t *LRU_Prob_to_evict(cache_t *cache, const request_t *req) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  return obj_to_evict;
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
static void LRU_Prob_evict(cache_t *cache, const request_t *req) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_remove_obj_base(cache, obj_to_evict, true);
}

static void LRU_Prob_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
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
static bool LRU_Prob_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  LRU_Prob_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *LRU_Prob_current_params(LRU_Prob_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "prob=%.4lf\n", params->prob);
  return params_str;
}

static void LRU_Prob_parse_params(cache_t *cache,
                                  const char *cache_specific_params) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "prob") == 0) {
      params->prob = (double)strtof(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", LRU_Prob_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}


#ifdef __cplusplus
}
#endif

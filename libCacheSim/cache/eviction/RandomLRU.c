//
//  RandomLRU.c
//  libCacheSim
//
//  Picks two objects at random and evicts the one that is the least recently
//  used RandomLRU eviction
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include <stdlib.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RandomLRU_params {
  int32_t n_samples;
} RandomLRU_params_t;

static const char *DEFAULT_CACHE_PARAMS = "n-samples=16";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void RandomLRU_free(cache_t *cache);
static bool RandomLRU_get(cache_t *cache, const request_t *req);
static cache_obj_t *RandomLRU_find(cache_t *cache, const request_t *req, const bool update_cache);
static cache_obj_t *RandomLRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *RandomLRU_to_evict(cache_t *cache, const request_t *req);
static void RandomLRU_evict(cache_t *cache, const request_t *req);
static bool RandomLRU_remove(cache_t *cache, const obj_id_t obj_id);
static void RandomLRU_parse_params(cache_t *cache, const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a RandomLRU cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params RandomLRU specific parameters, should be NULL
 */
cache_t *RandomLRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache = cache_struct_init("RandomLRU", ccache_params_copy, cache_specific_params);
  cache->cache_init = RandomLRU_init;
  cache->cache_free = RandomLRU_free;
  cache->get = RandomLRU_get;
  cache->find = RandomLRU_find;
  cache->insert = RandomLRU_insert;
  cache->to_evict = RandomLRU_to_evict;
  cache->evict = RandomLRU_evict;
  cache->remove = RandomLRU_remove;

  cache->eviction_params = (RandomLRU_params_t *)malloc(sizeof(RandomLRU_params_t));
  RandomLRU_params_t *params = (RandomLRU_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(RandomLRU_params_t));

  RandomLRU_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    RandomLRU_parse_params(cache, cache_specific_params);
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "RandomLRU-%d", params->n_samples);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void RandomLRU_free(cache_t *cache) { cache_struct_free(cache); }

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
static bool RandomLRU_get(cache_t *cache, const request_t *req) { return cache_get_base(cache, req); }

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

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
static cache_obj_t *RandomLRU_find(cache_t *cache, const request_t *req, const bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != NULL && update_cache) {
    obj->Random.last_access_vtime = cache->n_req;
  }

  return obj;
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
static cache_obj_t *RandomLRU_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Random.last_access_vtime = cache->n_req;

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
static cache_obj_t *RandomLRU_to_evict(cache_t *cache, const request_t *req) {
  // cache_obj_t *obj_to_evict1 = hashtable_rand_obj(cache->hashtable);
  // cache_obj_t *obj_to_evict2 = hashtable_rand_obj(cache->hashtable);
  // const int N = 16;
  // cache_objt_t *obj_to_evict[N];
  // for (int i = 0; i < N; i++) {
  //   obj_to_evict[i] = hashtable_rand_obj(cache->hashtable);
  // }
  // qsort(obj_to_evict, N, sizeof(cache_obj_t *), compare);

  // if (obj_to_evict1->Random.last_access_vtime < obj_to_evict2->Random.last_access_vtime)
  //   return obj_to_evict1;
  // else
  //   return obj_to_evict2;
  assert(false);
}

static int compare_access_time(const void *p1, const void *p2) {
  const cache_obj_t *obj1 = *(const cache_obj_t **)p1;
  const cache_obj_t *obj2 = *(const cache_obj_t **)p2;

  if (obj1->Random.last_access_vtime < obj2->Random.last_access_vtime) {
    return -1;
  } else if (obj1->Random.last_access_vtime > obj2->Random.last_access_vtime) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void RandomLRU_evict(cache_t *cache, const request_t *req) {
  const int N = 64;
  cache_obj_t *obj_to_evict[N];
  for (int i = 0; i < N; i++) {
    obj_to_evict[i] = hashtable_rand_obj(cache->hashtable);
  }
  qsort(obj_to_evict, N, sizeof(cache_obj_t *), compare_access_time);
  cache_evict_base(cache, obj_to_evict[0], true);
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
static bool RandomLRU_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  cache_remove_obj_base(cache, obj, true);

  return true;
}

static void RandomLRU_parse_params(cache_t *cache, const char *cache_specific_params) {
  RandomLRU_params_t *params = (RandomLRU_params_t *)(cache->eviction_params);

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

    if (strcasecmp(key, "n-sample") == 0 || strcasecmp(key, "n-samples") == 0) {
      params->n_samples = (int)strtol(value, &end, 0);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: n-samples=%d\n", params->n_samples);
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

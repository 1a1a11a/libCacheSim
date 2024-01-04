//
//  multi queue
//  each queue has a specified size and a specified ghost size
//  promotion upon hit
//
//  in progress, this is not finished
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
  cache_t **caches;
  cache_t **ghost_caches;
  int64_t n_caches;
  int64_t *cache_sizes;
  int64_t *ghost_sizes;

  request_t *req_local;

} myMQv1_params_t;

static const char *DEFAULT_PARAMS =
    "cache_sizes=0.25,0.25,0.25,0.25;ghost_sizes=0.75,0.75,0.75,0.75";
#define myMQv1_MAX_N_cache 16

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void myMQv1_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void myMQv1_free(cache_t *cache);
static bool myMQv1_get(cache_t *cache, const request_t *req);
static cache_obj_t *myMQv1_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *myMQv1_insert(cache_t *cache, const request_t *req);
static cache_obj_t *myMQv1_to_evict(cache_t *cache, const request_t *req);
static void myMQv1_evict(cache_t *cache, const request_t *req);
static bool myMQv1_remove(cache_t *cache, const obj_id_t obj_id);
static int64_t myMQv1_get_occupied_byte(const cache_t *cache);
static int64_t myMQv1_get_n_obj(const cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *myMQv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("myMQv1", ccache_params, cache_specific_params);
  cache->cache_init = myMQv1_init;
  cache->cache_free = myMQv1_free;
  cache->get = myMQv1_get;
  cache->find = myMQv1_find;
  cache->insert = myMQv1_insert;
  cache->evict = myMQv1_evict;
  cache->remove = myMQv1_remove;
  cache->to_evict = myMQv1_to_evict;
  cache->get_occupied_byte = myMQv1_get_occupied_byte;
  cache->get_n_obj = myMQv1_get_n_obj;
  cache->can_insert = cache_can_insert_default;
  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(myMQv1_params_t));
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
  params->req_local = new_request();

  if (cache_specific_params != NULL) {
    myMQv1_parse_params(cache, cache_specific_params);
  } else {
    myMQv1_parse_params(cache, DEFAULT_PARAMS);
  }

  params->caches = malloc(sizeof(cache_t *) * params->n_caches);
  params->ghost_caches = malloc(sizeof(cache_t *) * params->n_caches);
  common_cache_params_t ccache_params_local = ccache_params;
  for (int i = 0; i < params->n_caches; i++) {
    ccache_params_local.cache_size = params->cache_sizes[i];
    params->caches[i] = FIFO_init(ccache_params, NULL);
    params->ghost_caches[i] = FIFO_init(ccache_params_local, NULL);
  }

  // snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "myMQv1", 0.25);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void myMQv1_free(cache_t *cache) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
  for (int i = 0; i < params->n_caches; i++) {
    params->caches[i]->cache_free(params->caches[i]);
    params->ghost_caches[i]->cache_free(params->ghost_caches[i]);
  }
  free(params->caches);
  free(params->ghost_caches);
  free(params->cache_sizes);
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
  ERROR("todo");

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
  // myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  ERROR("todo");

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
  // myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  ERROR("todo");
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
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;

  for (int i = 0; i < params->n_caches; i++) {
    if (params->caches[i]->remove(params->caches[i], obj_id)) {
      return true;
    }
  }

  return true;
}

static int64_t myMQv1_get_occupied_byte(const cache_t *cache) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_caches; i++) {
    occupied_byte += params->caches[i]->get_occupied_byte(params->caches[i]);
  }
  return occupied_byte;
}

static int64_t myMQv1_get_n_obj(const cache_t *cache) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_caches; i++) {
    n_obj += params->caches[i]->get_n_obj(params->caches[i]);
  }
  return n_obj;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *myMQv1_current_params(cache_t *cache,
                                         myMQv1_params_t *params) {
  static __thread char params_str[128];
  int n = snprintf(params_str, 128, "n-caches=%ld;cache-size-ratio=%d",
                   (long)params->n_caches,
                   (int)(params->cache_sizes[0] * 100 / cache->cache_size));

  for (int i = 1; i < params->n_caches; i++) {
    n += snprintf(params_str + n, 128 - n, ":%d",
                  (int)(params->cache_sizes[i] * 100 / cache->cache_size));
  }
  n += snprintf(params_str + n, 128 - n, ";ghost-size-ratio=%d",
                (int)(params->ghost_sizes[0] * 100 / cache->cache_size));
  for (int i = 1; i < params->n_caches; i++) {
    n += snprintf(params_str + n, 128 - n, ":%d",
                  (int)(params->ghost_sizes[i] * 100 / cache->cache_size));
  }

  snprintf(cache->cache_name + n, 128 - n, "\n");

  return params_str;
}

static void myMQv1_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  myMQv1_params_t *params = (myMQv1_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "n-cache") == 0) {
      params->n_caches = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "cache-size-ratio") == 0) {
      int n_caches = 0;
      int64_t cache_size_sum = 0;
      int64_t cache_size_array[myMQv1_MAX_N_cache];
      char *v = strsep((char **)&value, ":");
      while (v != NULL) {
        cache_size_array[n_caches++] = (int64_t)strtol(v, &end, 0);
        cache_size_sum += cache_size_array[n_caches - 1];
        v = strsep((char **)&value, ":");
      }
      if (params->n_caches != 0 && params->n_caches != n_caches) {
        ERROR("n-cache and n-ghost-cache must be the same\n");
        exit(1);
      }
      params->n_caches = n_caches;
      params->cache_sizes = calloc(params->n_caches, sizeof(int64_t));
      for (int i = 0; i < n_caches; i++) {
        params->cache_sizes[i] = (int64_t)((double)cache_size_array[i] /
                                           cache_size_sum * cache->cache_size);
      }
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      int n_ghost_caches = 0;
      int64_t ghost_size_array[myMQv1_MAX_N_cache];
      char *v = strsep((char **)&value, ":");
      while (v != NULL) {
        ghost_size_array[n_ghost_caches++] = (int64_t)strtol(v, &end, 0);
        v = strsep((char **)&value, ":");
      }
      if (params->n_caches != 0 && params->n_caches != n_ghost_caches) {
        ERROR("n-cache and n-ghost-cache must be the same\n");
        exit(1);
      }
      params->ghost_sizes = calloc(params->n_caches, sizeof(int64_t));
      for (int i = 0; i < n_ghost_caches; i++) {
        params->ghost_sizes[i] =
            (int64_t)((double)ghost_size_array[i] / cache->cache_size);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", myMQv1_current_params(cache, params));
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

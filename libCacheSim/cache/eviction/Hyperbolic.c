/* Hyperbolic caching */

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Hyperbolic_params {
  int n_sample;
} Hyperbolic_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Hyperbolic_parse_params(cache_t *cache,
                                    const char *cache_specific_params);
static void Hyperbolic_free(cache_t *cache);
static bool Hyperbolic_get(cache_t *cache, const request_t *req);
static cache_obj_t *Hyperbolic_find(cache_t *cache, const request_t *req,
                                    const bool update_cache);
static cache_obj_t *Hyperbolic_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Hyperbolic_to_evict(cache_t *cache, const request_t *req);
static void Hyperbolic_evict(cache_t *cache, const request_t *req);
static bool Hyperbolic_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief initialize a cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *Hyperbolic_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  // reduce hash table size to make sampling faster
  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.hashpower = MAX(12, ccache_params_local.hashpower - 8);

  cache_t *cache = cache_struct_init("Hyperbolic", ccache_params_local, cache_specific_params);
  cache->cache_init = Hyperbolic_init;
  cache->cache_free = Hyperbolic_free;
  cache->get = Hyperbolic_get;
  cache->find = Hyperbolic_find;
  cache->insert = Hyperbolic_insert;
  cache->evict = Hyperbolic_evict;
  cache->remove = Hyperbolic_remove;
  cache->to_evict = Hyperbolic_to_evict;

  Hyperbolic_params_t *params = my_malloc(Hyperbolic_params_t);
  params->n_sample = 64;
  cache->eviction_params = params;

  if (cache_specific_params != NULL) {
    Hyperbolic_parse_params(cache, cache_specific_params);
  }

  if (ccache_params.consider_obj_metadata) {
    // freq + age
    cache->obj_md_size = 8 + 8;
  } else {
    cache->obj_md_size = 0;
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Hyperbolic_free(cache_t *cache) {
  Hyperbolic_params_t *params = cache->eviction_params;
  my_free(sizeof(Hyperbolic_params_t), params);
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
static bool Hyperbolic_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *Hyperbolic_find(cache_t *cache, const request_t *req,
                                    const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (update_cache && cache_obj) {
    cache_obj->hyperbolic.freq++;
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
static cache_obj_t *Hyperbolic_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *cached_obj = cache_insert_base(cache, req);
  cached_obj->hyperbolic.freq = 1;
  cached_obj->hyperbolic.vtime_enter_cache = cache->n_req;

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
static cache_obj_t *Hyperbolic_to_evict(cache_t *cache, const request_t *req) {
  Hyperbolic_params_t *params = cache->eviction_params;
  cache_obj_t *best_candidate = NULL, *sampled_obj;
  double best_candidate_score = 1.0e16, sampled_obj_score;
  for (int i = 0; i < params->n_sample; i++) {
    sampled_obj = hashtable_rand_obj(cache->hashtable);
    double age =
        (double)(cache->n_req - sampled_obj->hyperbolic.vtime_enter_cache);
    sampled_obj_score = 1.0e8 * (double)sampled_obj->hyperbolic.freq / age;
    if (best_candidate_score > sampled_obj_score) {
      best_candidate = sampled_obj;
      best_candidate_score = sampled_obj_score;
    }
  }

  cache->to_evict_candidate = best_candidate;
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  return best_candidate;
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
static void Hyperbolic_evict(cache_t *cache,
                             __attribute__((unused)) const request_t *req) {
  cache_obj_t *obj_to_evict = NULL;
  if (cache->to_evict_candidate_gen_vtime == cache->n_req) {
    obj_to_evict = cache->to_evict_candidate;
  } else {
    obj_to_evict = Hyperbolic_to_evict(cache, req);
  }
  cache->to_evict_candidate_gen_vtime = -1;

  if (obj_to_evict == NULL) {
    DEBUG_ASSERT(cache->n_obj == 0);
    WARN("no object can be evicted\n");
  }

  cache_evict_base(cache, obj_to_evict, true);
}

static void Hyperbolic_remove_obj(cache_t *cache, cache_obj_t *obj) {
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
static bool Hyperbolic_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  Hyperbolic_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *Hyperbolic_current_params(Hyperbolic_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-sample=%d\n", params->n_sample);
  return params_str;
}

static void Hyperbolic_parse_params(cache_t *cache,
                                    const char *cache_specific_params) {
  char *end;
  Hyperbolic_params_t *params = (Hyperbolic_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "n-sample") == 0) {
      params->n_sample = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", Hyperbolic_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name,
            key, Hyperbolic_current_params(params));
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

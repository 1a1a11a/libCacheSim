//
//  Quick demotion + lazy promotion v1
//
//  20% Ain + ARC
//  insert to ARC when evicting from Ain
//
//
//  LP_TwoQ.c
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
  cache_t *Ain;
  cache_t *Aout;
  cache_t *Am;
  bool hit_on_ghost;

  int64_t Ain_cache_size;
  int64_t Aout_cache_size;
  int64_t Am_cache_size;
  double Ain_size_ratio;
  double Aout_size_ratio;
  char Am_type[32];

  request_t *req_local;
} LP_TwoQ_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "Ain-size-ratio=0.25,Aout-size-ratio=0.5";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *LP_TwoQ_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);
static void LP_TwoQ_free(cache_t *cache);
static bool LP_TwoQ_get(cache_t *cache, const request_t *req);

static cache_obj_t *LP_TwoQ_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *LP_TwoQ_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LP_TwoQ_to_evict(cache_t *cache, const request_t *req);
static void LP_TwoQ_evict(cache_t *cache, const request_t *req);
static bool LP_TwoQ_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t LP_TwoQ_get_occupied_byte(const cache_t *cache);
static inline int64_t LP_TwoQ_get_n_obj(const cache_t *cache);
static inline bool LP_TwoQ_can_insert(cache_t *cache, const request_t *req);
static void LP_TwoQ_parse_params(cache_t *cache,
                                 const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief initialize cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *LP_TwoQ_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LP-TwoQv2", ccache_params, cache_specific_params);
  cache->cache_init = LP_TwoQ_init;
  cache->cache_free = LP_TwoQ_free;
  cache->get = LP_TwoQ_get;
  cache->find = LP_TwoQ_find;
  cache->insert = LP_TwoQ_insert;
  cache->evict = LP_TwoQ_evict;
  cache->remove = LP_TwoQ_remove;
  cache->to_evict = LP_TwoQ_to_evict;
  cache->get_n_obj = LP_TwoQ_get_n_obj;
  cache->get_occupied_byte = LP_TwoQ_get_occupied_byte;
  cache->can_insert = LP_TwoQ_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(LP_TwoQ_params_t));
  memset(cache->eviction_params, 0, sizeof(LP_TwoQ_params_t));
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  params->req_local = new_request();
  params->hit_on_ghost = false;

  LP_TwoQ_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    LP_TwoQ_parse_params(cache, cache_specific_params);
  }

  params->Ain_cache_size = ccache_params.cache_size * params->Ain_size_ratio;
  params->Aout_cache_size = ccache_params.cache_size * params->Aout_size_ratio;
  params->Am_cache_size = ccache_params.cache_size - params->Ain_cache_size;

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = params->Ain_cache_size;
  params->Ain = FIFO_init(ccache_params_local, NULL);

  ccache_params_local.cache_size = params->Aout_cache_size;
  params->Aout = FIFO_init(ccache_params_local, NULL);

  ccache_params_local.cache_size = params->Am_cache_size;
  // params->Am = LRU_init(ccache_params_local, NULL);
  params->Am = Clock_init(ccache_params_local, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "LP-TwoQ-Clock");

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LP_TwoQ_free(cache_t *cache) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->Ain->cache_free(params->Ain);
  params->Aout->cache_free(params->Aout);
  params->Am->cache_free(params->Am);
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
static bool LP_TwoQ_get(cache_t *cache, const request_t *req) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->Ain->get_occupied_byte(params->Ain) +
                   params->Am->get_occupied_byte(params->Am) <=
               cache->cache_size);
  bool cache_hit = cache_get_base(cache, req);
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
static cache_obj_t *LP_TwoQ_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;

  cache_obj_t *obj = params->Ain->find(params->Ain, req, false);

  // if update cache is false, we only check the Ain and Am
  if (!update_cache) {
    if (obj != NULL) {
      return obj;
    }
    obj = params->Am->find(params->Am, req, false);
    if (obj != NULL) {
      return obj;
    }
    return NULL;
  }

  /* update cache is true from now */
  params->hit_on_ghost = false;
  if (obj != NULL) {
    return obj;
  }

  if (params->Aout->remove(params->Aout, req->obj_id)) {
    // if object in Aout, remove will return true
    params->hit_on_ghost = true;
  }

  obj = params->Am->find(params->Am, req, update_cache);

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
static cache_obj_t *LP_TwoQ_insert(cache_t *cache, const request_t *req) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    params->Am->get(params->Am, req);
  } else {
    /* insert into the Ain */
    obj = params->Ain->insert(params->Ain, req);
  }
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
static cache_obj_t *LP_TwoQ_to_evict(cache_t *cache, const request_t *req) {
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
static void LP_TwoQ_evict(cache_t *cache, const request_t *req) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;

  cache_t *Ain = params->Ain;
  cache_t *Aout = params->Aout;
  cache_t *Am = params->Am;

  if (Ain->get_occupied_byte(Ain) > params->Ain_cache_size) {
    // evict from Ain cache
    cache_obj_t *obj = Ain->to_evict(Ain, req);
    assert(obj != NULL);
    // need to copy the object before it is evicted
    copy_cache_obj_to_request(params->req_local, obj);
    Aout->get(Aout, params->req_local);
    Ain->evict(Ain, req);
  } else {
    // // evict from Am and insert into Aout
    // cache_obj_t *obj = Am->to_evict(Ain, req);
    // assert(obj != NULL);
    // // need to copy the object before it is evicted
    // copy_cache_obj_to_request(params->req_local, obj);
    // Aout->get(Aout, params->req_local);
    // evict from Am
    Am->evict(Am, req);
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
static bool LP_TwoQ_remove(cache_t *cache, const obj_id_t obj_id) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->Ain->remove(params->Ain, obj_id);
  removed = removed || params->Aout->remove(params->Aout, obj_id);
  removed = removed || params->Am->remove(params->Am, obj_id);

  return removed;
}

static inline int64_t LP_TwoQ_get_occupied_byte(const cache_t *cache) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  return params->Ain->get_occupied_byte(params->Ain) +
         params->Am->get_occupied_byte(params->Am);
}

static inline int64_t LP_TwoQ_get_n_obj(const cache_t *cache) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;
  return params->Ain->get_n_obj(params->Ain) +
         params->Am->get_n_obj(params->Am);
}

static inline bool LP_TwoQ_can_insert(cache_t *cache, const request_t *req) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)cache->eviction_params;

  return req->obj_size <= params->Ain->cache_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *LP_TwoQ_current_params(LP_TwoQ_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "Ain-size-ratio=%.2lf,Aout-size-ratio=%.2lf\n",
           params->Ain_size_ratio, params->Aout_size_ratio);
  return params_str;
}

static void LP_TwoQ_parse_params(cache_t *cache,
                                 const char *cache_specific_params) {
  LP_TwoQ_params_t *params = (LP_TwoQ_params_t *)(cache->eviction_params);

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

    if (strcasecmp(key, "Ain-size-ratio") == 0) {
      params->Ain_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "Aout-size-ratio") == 0) {
      params->Aout_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", LP_TwoQ_current_params(params));
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

//
//  BeladyCompute.c
//  libCacheSim
//
//  sample object and compare reuse_distance / compute_intensity, then evict the
//  greatest one compute_intensity is taken from req->cost
//
//

#include <float.h>
#include <math.h>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define EXACT_Belady 1
static const char *DEFAULT_PARAMS = "n-sample=128";

typedef struct {
  // how many samples to take at each eviction
  int n_sample;
} BeladyCompute_params_t; /* BeladyCompute parameters */

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void BeladyCompute_parse_params(cache_t *cache,
                                       const char *cache_specific_params);
static void BeladyCompute_free(cache_t *cache);
static bool BeladyCompute_get(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyCompute_find(cache_t *cache, const request_t *req,
                                       const bool update_cache);
static cache_obj_t *BeladyCompute_insert(cache_t *cache, const request_t *req);
static cache_obj_t *BeladyCompute_to_evict(cache_t *cache,
                                           const request_t *req);
static void BeladyCompute_evict(cache_t *cache, const request_t *req);
static bool BeladyCompute_remove(cache_t *cache, const obj_id_t obj_id);
static void BeladyCompute_remove_obj(cache_t *cache, cache_obj_t *obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a BeladyCompute cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params BeladyCompute specific parameters, see
 * parse_params function or use -e "print" with the cachesim binary
 */
cache_t *BeladyCompute_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("BeladyCompute", ccache_params, cache_specific_params);

  cache->cache_init = BeladyCompute_init;
  cache->cache_free = BeladyCompute_free;
  cache->get = BeladyCompute_get;
  cache->find = BeladyCompute_find;
  cache->insert = BeladyCompute_insert;
  cache->evict = BeladyCompute_evict;
  cache->remove = BeladyCompute_remove;
  cache->to_evict = BeladyCompute_to_evict;

  BeladyCompute_params_t *params =
      (BeladyCompute_params_t *)malloc(sizeof(BeladyCompute_params_t));
  cache->eviction_params = params;

  BeladyCompute_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    BeladyCompute_parse_params(cache, cache_specific_params);
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void BeladyCompute_free(cache_t *cache) {
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
static bool BeladyCompute_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *BeladyCompute_find(cache_t *cache, const request_t *req,
                                       const bool update_cache) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, req->obj_id);
  if (obj != NULL && likely(update_cache)) {
    // store the compute intensity in the cache object for eviction decisions
    if (req->n_features > 0) {
      obj->cost = req->cost;
    }

    if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
      obj->Belady.next_access_vtime = -1;
    } else {
      obj->Belady.next_access_vtime = req->next_access_vtime;
    }
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
static cache_obj_t *BeladyCompute_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);

  // store the compute intensity for eviction decisions
  obj->cost = req->cost;

  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    obj->Belady.next_access_vtime = -1;
  } else {
    obj->Belady.next_access_vtime = req->next_access_vtime;
  }

  return obj;
}

#ifdef EXACT_Belady
struct hash_iter_user_data {
  int64_t curr_vtime;
  cache_obj_t *to_evict_obj;
  double max_score;
};

static void hashtable_iter_Belady_compute(cache_obj_t *cache_obj,
                                          void *userdata) {
  struct hash_iter_user_data *iter_userdata =
      (struct hash_iter_user_data *)userdata;
  if (iter_userdata->max_score == DBL_MAX) return;

  double obj_score;
  if (cache_obj->Belady.next_access_vtime == -1) {
    obj_score = DBL_MAX;
  } else {
    int64_t time_diff =
        cache_obj->Belady.next_access_vtime - iter_userdata->curr_vtime;
    int32_t compute_intensity = cache_obj->cost;
    if (compute_intensity <= 0)
      compute_intensity = 1;  // avoid division by zero
    obj_score = (double)time_diff / (double)compute_intensity;
  }

  if (obj_score > iter_userdata->max_score) {
    iter_userdata->to_evict_obj = cache_obj;
    iter_userdata->max_score = obj_score;
  }
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
static cache_obj_t *BeladyCompute_to_evict(cache_t *cache,
                                           const request_t *req) {
  struct hash_iter_user_data iter_userdata;
  iter_userdata.curr_vtime = cache->n_req;
  iter_userdata.max_score = 0.0;
  iter_userdata.to_evict_obj = NULL;

  hashtable_foreach(cache->hashtable, hashtable_iter_Belady_compute,
                    &iter_userdata);

  return iter_userdata.to_evict_obj;
}

#else
static cache_obj_t *BeladyCompute_to_evict(cache_t *cache,
                                           const request_t *req) {
  BeladyCompute_params_t *params =
      (BeladyCompute_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = NULL, *sampled_obj;
  double obj_to_evict_score = -DBL_MAX, sampled_obj_score = -1;

  for (int i = 0; i < params->n_sample; i++) {
    sampled_obj = hashtable_rand_obj(cache->hashtable);
    if (sampled_obj->Belady.next_access_vtime == -1) {
      sampled_obj_score = DBL_MAX;
      obj_to_evict = sampled_obj;
      break;  // no need to continue sampling if we find an object that will
              // never be accessed again
    } else {
      int64_t time_diff = sampled_obj->Belady.next_access_vtime - cache->n_req;
      int32_t compute_intensity = sampled_obj->cost;
      if (compute_intensity <= 0) {
        ERROR(
            "BeladyCompute_to_evict: compute_intensity is %d for obj_id %lu, "
            "this may cause division by zero, default to 1\n",
            compute_intensity, (unsigned long)sampled_obj->obj_id);

        compute_intensity = 1;  // avoid division by zero
      }

      if (time_diff <= 0) {
        // ERROR(
        //     "BeladyCompute_to_evict: time_diff is %ld for obj_id %lu, this may "
        //     "cause "
        //     "negative or zero score, default to 1\n",
        //     (long)time_diff, (unsigned long)sampled_obj->obj_id);
        sampled_obj_score = -DBL_MAX / 2;
      } else {
        // Use log to handle large numbers and avoid overflow
        sampled_obj_score =
            log((double)time_diff) - log((double)compute_intensity);
      }
    }

    if (obj_to_evict == NULL || obj_to_evict_score < sampled_obj_score) {
      obj_to_evict = sampled_obj;
      obj_to_evict_score = sampled_obj_score;
    }
  }

  if (obj_to_evict == NULL) {
    WARN(
        "BeladyCompute_to_evict: obj_to_evict is NULL, "
        "maybe cache size is too small or hash power too large, "
        "current hash table size %llu, n_obj %llu, cache size %lld, request "
        "size "
        "%lld, and %d samples "
        "obj_to_evict_score %.4lf sampled_obj_score %.4lf\n",
        (unsigned long long)hashsize(cache->hashtable->hashpower),
        (unsigned long long)cache->get_n_obj(cache),
        (long long)cache->cache_size, (long long)req->obj_size,
        params->n_sample, obj_to_evict_score, sampled_obj_score);
    return BeladyCompute_to_evict(cache, req);
  }

  return obj_to_evict;
}
#endif

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void BeladyCompute_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict = BeladyCompute_to_evict(cache, req);
  cache_evict_base(cache, obj_to_evict, true);
}

bool BeladyCompute_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  cache_remove_obj_base(cache, obj, true);
  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief print the default parameters
 *
 */
static const char *BeladyCompute_current_params(
    BeladyCompute_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-sample=%d\n", params->n_sample);
  return params_str;
}

/**
 * parse the given parameters
 * input parameter is a string,
 * to see the default parameters, use current_params()
 * or use -e "print" with cachesim
 */
static void BeladyCompute_parse_params(cache_t *cache,
                                       const char *cache_specific_params) {
  BeladyCompute_params_t *params =
      (BeladyCompute_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by '=' */
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
      printf("current parameters: %s\n", BeladyCompute_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name,
            key, BeladyCompute_current_params(params));
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

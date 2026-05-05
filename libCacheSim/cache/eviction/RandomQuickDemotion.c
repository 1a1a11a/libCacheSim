//
//  RandomQuickDemotion.c
//  libCacheSim
//
//  Random sampling eviction with quick demotion of one-hit-wonder objects.
//  Ported from vllm/v1/core/free_block_manager.py:
//    RandomQuickDemotionFreeBlockManager (multiplier mode).
//
//  Score = (freq + 1) / (n_req - last_access_vtime + 1) * cost
//  One-hit-wonder penalty: if freq == 0, score *= ONE_HIT_PENALTY (default 0.1)
//  Lower score = evict first.
//
//  Notes on the port:
//  - libCacheSim has no radix tree, so the original "is_leaf" gate from
//    vllm has no analogue. We drop it and only check the access-count
//    condition.
//  - In libCacheSim, freq is 0 on insert and increments on hit. In vllm
//    access_count starts at 1. So vllm's `access_count <= 1`
//    (one-hit-wonder, never reused) maps to `freq == 0` here.
//

#include <float.h>
#include <math.h>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

static const char *DEFAULT_PARAMS = "n-sample=128,one-hit-penalty=0.1";

typedef struct {
  int n_sample;
  double one_hit_penalty;
} RandomQuickDemotion_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void RandomQuickDemotion_parse_params(
    cache_t *cache, const char *cache_specific_params);
static void RandomQuickDemotion_free(cache_t *cache);
static bool RandomQuickDemotion_get(cache_t *cache, const request_t *req);
static cache_obj_t *RandomQuickDemotion_find(cache_t *cache,
                                             const request_t *req,
                                             const bool update_cache);
static cache_obj_t *RandomQuickDemotion_insert(cache_t *cache,
                                               const request_t *req);
static cache_obj_t *RandomQuickDemotion_to_evict(cache_t *cache,
                                                 const request_t *req);
static void RandomQuickDemotion_evict(cache_t *cache, const request_t *req);
static bool RandomQuickDemotion_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                       init, free, get                         ****
// ****                                                               ****
// ***********************************************************************

cache_t *RandomQuickDemotion_init(const common_cache_params_t ccache_params,
                                  const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache = cache_struct_init("RandomQuickDemotion", ccache_params_copy,
                                     cache_specific_params);

  cache->cache_init = RandomQuickDemotion_init;
  cache->cache_free = RandomQuickDemotion_free;
  cache->get = RandomQuickDemotion_get;
  cache->find = RandomQuickDemotion_find;
  cache->insert = RandomQuickDemotion_insert;
  cache->evict = RandomQuickDemotion_evict;
  cache->remove = RandomQuickDemotion_remove;
  cache->to_evict = RandomQuickDemotion_to_evict;

  RandomQuickDemotion_params_t *params =
      (RandomQuickDemotion_params_t *)malloc(
          sizeof(RandomQuickDemotion_params_t));
  cache->eviction_params = params;

  RandomQuickDemotion_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    RandomQuickDemotion_parse_params(cache, cache_specific_params);
  }

  return cache;
}

static void RandomQuickDemotion_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

static bool RandomQuickDemotion_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *RandomQuickDemotion_find(cache_t *cache,
                                             const request_t *req,
                                             const bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != NULL && likely(update_cache)) {
    obj->Random.last_access_vtime = cache->n_req;
  }
  return obj;
}

static cache_obj_t *RandomQuickDemotion_insert(cache_t *cache,
                                               const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Random.last_access_vtime = cache->n_req;
  return obj;
}

static inline double _rqd_score(cache_t *cache, cache_obj_t *obj,
                                double one_hit_penalty) {
  double recency =
      (double)(cache->n_req - obj->Random.last_access_vtime + 1);
  double score =
      (double)obj->cost * (double)(obj->misc.freq + 1) / recency;
  if (obj->misc.freq == 0) {
    score *= one_hit_penalty;
  }
  return score;
}

static cache_obj_t *RandomQuickDemotion_to_evict(cache_t *cache,
                                                 const request_t *req) {
  RandomQuickDemotion_params_t *params =
      (RandomQuickDemotion_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = NULL;
  double min_score = DBL_MAX;

  for (int i = 0; i < params->n_sample; i++) {
    cache_obj_t *obj = hashtable_rand_obj(cache->hashtable);
    if (obj == NULL) continue;
    double score = _rqd_score(cache, obj, params->one_hit_penalty);
    if (score < min_score) {
      min_score = score;
      obj_to_evict = obj;
    }
  }

  if (obj_to_evict == NULL) {
    WARN(
        "RandomQuickDemotion_to_evict: obj_to_evict is NULL, "
        "maybe cache size is too small or hash power too large, "
        "current hash table size %llu, n_obj %llu, cache size %lld, request "
        "size %lld, and %d samples\n",
        (unsigned long long)hashsize(cache->hashtable->hashpower),
        (unsigned long long)cache->get_n_obj(cache),
        (long long)cache->cache_size, (long long)req->obj_size,
        params->n_sample);
    return RandomQuickDemotion_to_evict(cache, req);
  }

  return obj_to_evict;
}

static void RandomQuickDemotion_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict = RandomQuickDemotion_to_evict(cache, req);
  cache_evict_base(cache, obj_to_evict, true);
}

static bool RandomQuickDemotion_remove(cache_t *cache, const obj_id_t obj_id) {
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

static const char *RandomQuickDemotion_current_params(
    RandomQuickDemotion_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-sample=%d,one-hit-penalty=%.4f\n",
           params->n_sample, params->one_hit_penalty);
  return params_str;
}

static void RandomQuickDemotion_parse_params(
    cache_t *cache, const char *cache_specific_params) {
  RandomQuickDemotion_params_t *params =
      (RandomQuickDemotion_params_t *)cache->eviction_params;
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
    } else if (strcasecmp(key, "one-hit-penalty") == 0) {
      params->one_hit_penalty = strtod(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n",
             RandomQuickDemotion_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name,
            key, RandomQuickDemotion_current_params(params));
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

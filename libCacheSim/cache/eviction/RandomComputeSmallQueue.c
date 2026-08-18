//
//  RandomComputeSmallQueue.c
//  libCacheSim
//
//  Random-sampling eviction that demotes one-hit wonders.
//
//  Score = cost / max(1, n_req - last_access_vtime)
//  One-hit-wonder demotion: if freq == 0, score *= one-hit-penalty (default 0.1)
//  Lower score = evict first.
//
//  The score is the same as RandomCompute and PartialNodeRandomCompute: cost
//  over recency, with no frequency factor -- frequency carries little signal on
//  these traces once the cache is large enough to hold the working set, and
//  where it does help it is better spent as an admission decision than as a
//  multiplier.
//
//  Frequency survives only as the gate on the demotion: a block that has never
//  been reused since it was inserted is worth an order of magnitude less than
//  its cost/recency suggests, so it goes first. That is a small queue expressed
//  as a score penalty rather than as a physical queue -- no separate FIFO, no
//  ghost, no promotion step, and no second data structure to keep in sync.
//
//  NOTE: despite the name, this is not a port of vLLM's
//  RandomSmallQueueFreeBlockManager, which keeps a real FIFO queue and a ghost
//  list. The physical-queue form lives in PartialNodeRandomComputeSmallQueue.
//  This one is the cheap approximation of the same idea, ported from vLLM's
//  RandomQuickDemotion.
//
//  Notes on the port:
//  - libCacheSim has no radix tree, so the original "is_leaf" gate from
//    vllm has no analogue. We drop it and only check the access-count
//    condition.
//  - In libCacheSim, freq is 0 on insert and increments on hit, so freq == 0
//    means "never reused since admission".
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
} RandomComputeSmallQueue_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void RandomComputeSmallQueue_parse_params(
    cache_t *cache, const char *cache_specific_params);
static void RandomComputeSmallQueue_free(cache_t *cache);
static bool RandomComputeSmallQueue_get(cache_t *cache, const request_t *req);
static cache_obj_t *RandomComputeSmallQueue_find(cache_t *cache,
                                             const request_t *req,
                                             const bool update_cache);
static cache_obj_t *RandomComputeSmallQueue_insert(cache_t *cache,
                                               const request_t *req);
static cache_obj_t *RandomComputeSmallQueue_to_evict(cache_t *cache,
                                                 const request_t *req);
static void RandomComputeSmallQueue_evict(cache_t *cache, const request_t *req);
static bool RandomComputeSmallQueue_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                       init, free, get                         ****
// ****                                                               ****
// ***********************************************************************

cache_t *RandomComputeSmallQueue_init(const common_cache_params_t ccache_params,
                                  const char *cache_specific_params) {
  common_cache_params_t ccache_params_copy = ccache_params;
  ccache_params_copy.hashpower = MAX(12, ccache_params_copy.hashpower - 8);

  cache_t *cache = cache_struct_init("RandomComputeSmallQueue", ccache_params_copy,
                                     cache_specific_params);

  cache->cache_init = RandomComputeSmallQueue_init;
  cache->cache_free = RandomComputeSmallQueue_free;
  cache->get = RandomComputeSmallQueue_get;
  cache->find = RandomComputeSmallQueue_find;
  cache->insert = RandomComputeSmallQueue_insert;
  cache->evict = RandomComputeSmallQueue_evict;
  cache->remove = RandomComputeSmallQueue_remove;
  cache->to_evict = RandomComputeSmallQueue_to_evict;

  RandomComputeSmallQueue_params_t *params =
      (RandomComputeSmallQueue_params_t *)malloc(
          sizeof(RandomComputeSmallQueue_params_t));
  cache->eviction_params = params;

  RandomComputeSmallQueue_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    RandomComputeSmallQueue_parse_params(cache, cache_specific_params);
  }

  return cache;
}

static void RandomComputeSmallQueue_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

static bool RandomComputeSmallQueue_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *RandomComputeSmallQueue_find(cache_t *cache,
                                             const request_t *req,
                                             const bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != NULL && likely(update_cache)) {
    obj->Random.last_access_vtime = cache->n_req;
  }
  return obj;
}

static cache_obj_t *RandomComputeSmallQueue_insert(cache_t *cache,
                                               const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Random.last_access_vtime = cache->n_req;
  return obj;
}

static inline double _rcsq_score(cache_t *cache, cache_obj_t *obj,
                                double one_hit_penalty) {
  /* cost / recency, byte-for-byte the same as _rc_cost() in RandomCompute.c:
   * no frequency factor, and the same max(1, age) floor rather than age + 1,
   * so the two differ only in the demotion below. */
  int64_t age = cache->n_req - obj->Random.last_access_vtime;
  int64_t recency = age > 1 ? age : 1;
  double score = (double)obj->cost / (double)recency;
  if (obj->misc.freq == 0) {
    score *= one_hit_penalty;
  }
  return score;
}

static cache_obj_t *RandomComputeSmallQueue_to_evict(cache_t *cache,
                                                 const request_t *req) {
  RandomComputeSmallQueue_params_t *params =
      (RandomComputeSmallQueue_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = NULL;
  double min_score = DBL_MAX;

  for (int i = 0; i < params->n_sample; i++) {
    cache_obj_t *obj = hashtable_rand_obj(cache->hashtable);
    if (obj == NULL) continue;
    double score = _rcsq_score(cache, obj, params->one_hit_penalty);
    if (score < min_score) {
      min_score = score;
      obj_to_evict = obj;
    }
  }

  if (obj_to_evict == NULL) {
    WARN(
        "RandomComputeSmallQueue_to_evict: obj_to_evict is NULL, "
        "maybe cache size is too small or hash power too large, "
        "current hash table size %llu, n_obj %llu, cache size %lld, request "
        "size %lld, and %d samples\n",
        (unsigned long long)hashsize(cache->hashtable->hashpower),
        (unsigned long long)cache->get_n_obj(cache),
        (long long)cache->cache_size, (long long)req->obj_size,
        params->n_sample);
    return RandomComputeSmallQueue_to_evict(cache, req);
  }

  return obj_to_evict;
}

static void RandomComputeSmallQueue_evict(cache_t *cache, const request_t *req) {
  cache_obj_t *obj_to_evict = RandomComputeSmallQueue_to_evict(cache, req);
  cache_evict_base(cache, obj_to_evict, true);
}

static bool RandomComputeSmallQueue_remove(cache_t *cache, const obj_id_t obj_id) {
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

static const char *RandomComputeSmallQueue_current_params(
    RandomComputeSmallQueue_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-sample=%d,one-hit-penalty=%.4f\n",
           params->n_sample, params->one_hit_penalty);
  return params_str;
}

static void RandomComputeSmallQueue_parse_params(
    cache_t *cache, const char *cache_specific_params) {
  RandomComputeSmallQueue_params_t *params =
      (RandomComputeSmallQueue_params_t *)cache->eviction_params;
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
             RandomComputeSmallQueue_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name,
            key, RandomComputeSmallQueue_current_params(params));
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

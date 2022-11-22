/* Hyperbolic caching */

#include "../../include/libCacheSim/evictionAlgo/Hyperbolic.h"

#include "../../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Hyperbolic_params {
  int n_sample;
} Hyperbolic_params_t;

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

cache_t *Hyperbolic_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Hyperbolic", ccache_params);
  cache->cache_init = Hyperbolic_init;
  cache->cache_free = Hyperbolic_free;
  cache->get = Hyperbolic_get;
  cache->check = Hyperbolic_check;
  cache->insert = Hyperbolic_insert;
  cache->evict = Hyperbolic_evict;
  cache->remove = Hyperbolic_remove;
  cache->to_evict = Hyperbolic_to_evict;
  cache->init_params = cache_specific_params;

  Hyperbolic_params_t *params = my_malloc(Hyperbolic_params_t);
  params->n_sample = 64;
  cache->eviction_params = params;

  if (cache_specific_params != NULL) {
    Hyperbolic_parse_params(cache, cache_specific_params);
  }

  if (ccache_params.consider_obj_metadata) {
    // freq + age
    cache->per_obj_metadata_size = 8 + 8;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  return cache;
}

void Hyperbolic_free(cache_t *cache) {
  Hyperbolic_params_t *params = cache->eviction_params;
  my_free(sizeof(Hyperbolic_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e Hyperbolic_check(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  cache_obj_t *cached_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cached_obj);

  if (!update_cache) return ret;

  if (ret == cache_ck_hit) {
    cached_obj->hyperbolic.freq++;

    return cache_ck_hit;
  }

  return cache_ck_miss;
}

cache_ck_res_e Hyperbolic_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e ret = cache_get_base(cache, req);

  return ret;
}

cache_obj_t *Hyperbolic_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *cached_obj = cache_insert_base(cache, req);
  cached_obj->hyperbolic.freq = 1;
  cached_obj->hyperbolic.vtime_enter_cache = cache->n_req;

  return cached_obj;
}

cache_obj_t *Hyperbolic_to_evict(cache_t *cache) {
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

  DEBUG_ASSERT(best_candidate != NULL);
  return best_candidate;
}

void Hyperbolic_evict(cache_t *cache,
                      __attribute__((unused)) const request_t *req,
                      cache_obj_t *evicted_obj) {
  cache_obj_t *obj_to_evict = Hyperbolic_to_evict(cache);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  cache_remove_obj_base(cache, obj_to_evict);
}

void Hyperbolic_remove_obj(cache_t *cache, cache_obj_t *obj) {
  cache_remove_obj_base(cache, obj);
}

void Hyperbolic_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("obj to remove is not in the cache\n");
    return;
  }

  Hyperbolic_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

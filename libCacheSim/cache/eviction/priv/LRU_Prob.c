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

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"
#include "../../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LRU_Prob_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  double prob;
  int threshold;
} LRU_Prob_params_t;

bool LRU_Prob_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

bool LRU_Prob_check(cache_t *cache, const request_t *req,
                    const bool update_cache) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;

  cache_obj_t *cached_obj = NULL;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cached_obj);
  if (cached_obj != NULL && likely(update_cache)) {
    cached_obj->misc.freq += 1;
    cached_obj->misc.last_access_rtime = req->clock_time;
    cached_obj->misc.last_access_vtime = cache->n_req;
    cached_obj->next_access_vtime = req->next_access_vtime;

    if (next_rand() % params->threshold == 0) {
      move_obj_to_head(&params->q_head, &params->q_tail, cached_obj);
    }
  }

  return cache_hit;
}

cache_obj_t *LRU_Prob_insert(cache_t *cache, const request_t *req) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  return obj;
}

cache_obj_t *LRU_Prob_to_evict(cache_t *cache) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  return obj_to_evict;
}

void LRU_Prob_evict(cache_t *cache, const request_t *req,
                    cache_obj_t *evicted_obj) {
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_remove_obj_base(cache, obj_to_evict, true);
}

void LRU_Prob_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  LRU_Prob_params_t *params = (LRU_Prob_params_t *)cache->eviction_params;

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
}

bool LRU_Prob_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    return false;
  }

  LRU_Prob_remove_obj(cache, obj);

  return true;
}

// ######################## setup function ###########################
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

void LRU_Prob_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *LRU_Prob_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRU_Prob", ccache_params);
  cache->cache_init = LRU_Prob_init;
  cache->cache_free = LRU_Prob_free;
  cache->get = LRU_Prob_get;
  cache->check = LRU_Prob_check;
  cache->insert = LRU_Prob_insert;
  cache->evict = LRU_Prob_evict;
  cache->remove = LRU_Prob_remove;
  cache->to_evict = LRU_Prob_to_evict;
  cache->init_params = cache_specific_params;
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

#ifdef __cplusplus
}
#endif

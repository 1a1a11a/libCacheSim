//
//  FIFO_Merge scans N objects and retains M objects, evict the rest
//
//
//  FIFO_Merge.c
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../include/libCacheSim/evictionAlgo/FIFO_Merge.h"

#include <assert.h>

#include "../../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sort_list_node {
  double metric;
  cache_obj_t *cache_obj;
};

static inline int cmp_list_node(const void *a0, const void *b0) {
  struct sort_list_node *a = (struct sort_list_node *)a0;
  struct sort_list_node *b = (struct sort_list_node *)b0;

  if (a->metric > b->metric) {
    return 1;
  } else if (a->metric < b->metric) {
    return -1;
  } else {
    return 0;
  }
}

typedef enum {
  RETAIN_POLICY_RECENCY = 0,
  RETAIN_POLICY_FREQUENCY,
  RETAIN_POLICY_BELADY,
  RETAIN_NONE
} retain_policy_t;

static char *retain_policy_names[] = {"RECENCY", "FREQUENCY", "BELADY", "None"};

typedef struct FIFO_Merge_params {
  // points to the eviction position
  cache_obj_t *next_to_merge;
  // the number of object to examine at each eviction
  int n_exam_obj;
  // of the n_exam_obj, we keep n_keep_obj and evict the rest
  int n_keep_obj;
  // used to sort the n_exam_obj objects
  struct sort_list_node *metric_list;
  // the policy to determine the n_keep_obj objects
  retain_policy_t retain_policy;
} FIFO_Merge_params_t;

static const char *FIFO_Merge_current_params(FIFO_Merge_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-exam=%d, n-keep=%d, retain-policy=%s",
           params->n_exam_obj, params->n_keep_obj,
           retain_policy_names[params->retain_policy]);
  return params_str;
}

static void FIFO_Merge_parse_params(cache_t *cache,
                                    const char *cache_specific_params) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;

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
    if (strcasecmp(key, "retain-policy") == 0) {
      if (strcasecmp(value, "freq") == 0 || strcasecmp(value, "frequency") == 0)
        params->retain_policy = RETAIN_POLICY_FREQUENCY;
      else if (strcasecmp(value, "recency") == 0)
        params->retain_policy = RETAIN_POLICY_RECENCY;
      else if (strcasecmp(value, "belady") == 0 ||
               strcasecmp(value, "optimal") == 0)
        params->retain_policy = RETAIN_POLICY_BELADY;
      else if (strcasecmp(value, "none") == 0) {
        params->retain_policy = RETAIN_NONE;
        params->n_keep_obj = 0;
      } else {
        ERROR("unknown retain-policy %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "n-exam") == 0) {
      params->n_exam_obj = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "n-keep") == 0) {
      params->n_keep_obj = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("%s parameters: %s\n", cache->cache_name,
             FIFO_Merge_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

cache_t *FIFO_Merge_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("FIFO_Merge", ccache_params);
  cache->cache_init = FIFO_Merge_init;
  cache->cache_free = FIFO_Merge_free;
  cache->get = FIFO_Merge_get;
  cache->check = FIFO_Merge_check;
  cache->insert = FIFO_Merge_insert;
  cache->evict = FIFO_Merge_evict;
  cache->remove = FIFO_Merge_remove;
  cache->to_evict = FIFO_Merge_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 4;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  FIFO_Merge_params_t *params = my_malloc(FIFO_Merge_params_t);
  memset(params, 0, sizeof(FIFO_Merge_params_t));
  cache->eviction_params = params;

  /* TODO: can we make this parameter adaptive to trace? */
  params->n_exam_obj = 100;
  params->n_keep_obj = params->n_exam_obj / 2;
  params->retain_policy = RETAIN_POLICY_FREQUENCY;
  params->next_to_merge = NULL;

  if (cache_specific_params != NULL) {
    FIFO_Merge_parse_params(cache, cache_specific_params);
  }

  assert(params->n_exam_obj > 0 && params->n_keep_obj >= 0);
  assert(params->n_keep_obj <= params->n_exam_obj);

  snprintf(cache->cache_name, 32, "FIFO_Merge_%s",
           retain_policy_names[params->retain_policy]);
  params->metric_list = my_malloc_n(struct sort_list_node, params->n_exam_obj);

  return cache;
}

void FIFO_Merge_free(cache_t *cache) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;
  my_free(sizeof(struct sort_list_node) * params->n_exam_obj,
          params->metric_list);
  my_free(sizeof(FIFO_Merge_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e FIFO_Merge_check(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e res = cache_check_base(cache, req, update_cache, &cache_obj);

  if (res == cache_ck_hit) {
    cache_obj->FIFO_Merge.freq++;
    cache_obj->FIFO_Merge.last_access_vtime = cache->n_req;
    cache_obj->FIFO_Merge.next_access_vtime = req->next_access_vtime;
  }

  return res;
}

cache_ck_res_e FIFO_Merge_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

static inline double belady_metric(cache_t *cache, cache_obj_t *cache_obj) {
  if (cache_obj->FIFO_Merge.next_access_vtime == -1 ||
      cache_obj->FIFO_Merge.next_access_vtime == INT64_MAX)
    return -1;
  return 1.0e12 / (cache_obj->FIFO_Merge.next_access_vtime - cache->n_req) /
         (double)cache_obj->obj_size;
}

static inline double freq_metric(cache_t *cache, cache_obj_t *cache_obj) {
  /* we add a small rand number to distinguish objects with frequency 0 or same
   * frequency */
  double r = (double)(next_rand() % 1000) / 10000.0;
  return 1.0e6 * ((double)cache_obj->FIFO_Merge.freq + r) /
         (double)cache_obj->obj_size;
}

static inline double recency_metric(cache_t *cache, cache_obj_t *cache_obj) {
  return 1.0e12 /
         (double)(cache->n_req - cache_obj->FIFO_Merge.last_access_vtime) /
         (double)cache_obj->obj_size;
}

static double retain_metric(cache_t *cache, cache_obj_t *cache_obj) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;

  switch (params->retain_policy) {
    case RETAIN_POLICY_FREQUENCY:
      return freq_metric(cache, cache_obj);
    case RETAIN_POLICY_RECENCY:
      return recency_metric(cache, cache_obj);
    case RETAIN_POLICY_BELADY:
      return belady_metric(cache, cache_obj);
    default:
      break;
  }
}

cache_obj_t *FIFO_Merge_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *cache_obj = cache_insert_LRU(cache, req);
  cache_obj->FIFO_Merge.freq = 0;
  cache_obj->FIFO_Merge.last_access_vtime = cache->n_req;
  cache_obj->FIFO_Merge.next_access_vtime = req->next_access_vtime;

  return cache_obj;
}

cache_obj_t *FIFO_Merge_to_evict(cache_t *cache) {
  ERROR("Undefined! Multiple objs will be evicted\n");
  abort();
  return NULL;
}

void FIFO_Merge_evict(cache_t *cache, const request_t *req,
                      cache_obj_t *evicted_obj) {
  assert(evicted_obj == NULL);

  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;

  // collect metric for n_exam obj, we will keep objects with larger metric
  int n_loop = 0;
  cache_obj_t *cache_obj = params->next_to_merge;
  if (cache_obj == NULL) {
    params->next_to_merge = cache->q_tail;
    cache_obj = cache->q_tail;
    n_loop = 1;
  }

  if (cache->n_obj <= params->n_exam_obj) {
    // just evict one object
    cache_obj_t *cache_obj = params->next_to_merge->queue.prev;
    FIFO_Merge_remove_obj(cache, params->next_to_merge);
    params->next_to_merge = cache_obj;

    return;
  }

  for (int i = 0; i < params->n_exam_obj; i++) {
    assert(cache_obj != NULL);
    params->metric_list[i].metric = retain_metric(cache, cache_obj);
    params->metric_list[i].cache_obj = cache_obj;
    cache_obj = cache_obj->queue.prev;

    //  TODO: wrap back to the head of the list early before reaching the end of
    //  the list
    if (cache_obj == NULL) {
      cache_obj = cache->q_tail;
      DEBUG_ASSERT(n_loop++ <= 2);
    }
  }
  params->next_to_merge = cache_obj;

  // sort metrics
  qsort(params->metric_list, params->n_exam_obj, sizeof(struct sort_list_node),
        cmp_list_node);

  // remove objects
  int n_evict = params->n_exam_obj - params->n_keep_obj;
  for (int i = 0; i < n_evict; i++) {
    cache_obj = params->metric_list[i].cache_obj;
    FIFO_Merge_remove_obj(cache, cache_obj);
  }

  for (int i = n_evict; i < params->n_exam_obj; i++) {
    cache_obj = params->metric_list[i].cache_obj;
    cache_obj->FIFO_Merge.freq = (cache_obj->FIFO_Merge.freq + 1) / 2;
  }
}

void FIFO_Merge_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  if (obj_to_remove == NULL) {
    ERROR("remove NULL from cache\n");
    abort();
  }

  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove);
}

void FIFO_Merge_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached\n", obj_id);
    return;
  }
  FIFO_Merge_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

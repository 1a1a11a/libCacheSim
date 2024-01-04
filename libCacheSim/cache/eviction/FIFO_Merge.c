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

#include <assert.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sort_list_node {
  double metric;
  cache_obj_t *cache_obj;
};

typedef enum {
  RETAIN_POLICY_RECENCY = 0,
  RETAIN_POLICY_FREQUENCY,
  RETAIN_POLICY_BELADY,
  RETAIN_NONE
} retain_policy_t;

static char *retain_policy_names[] = {"RECENCY", "FREQUENCY", "BELADY", "None"};

typedef struct FIFO_Merge_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the eviction position
  cache_obj_t *next_to_exam;
  // the number of object to examine at each eviction
  int n_exam_obj;
  // of the n_exam_obj, we keep n_keep_obj and evict the rest
  int n_keep_obj;
  // used to sort the n_exam_obj objects
  struct sort_list_node *metric_list;
  // the policy to determine the n_keep_obj objects
  retain_policy_t retain_policy;
  int pos_in_metric_list;
} FIFO_Merge_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void FIFO_Merge_parse_params(cache_t *cache,
                                     const char *cache_specific_params);
static void FIFO_Merge_free(cache_t *cache);
static bool FIFO_Merge_get(cache_t *cache, const request_t *req);
static cache_obj_t *FIFO_Merge_find(cache_t *cache, const request_t *req,
                                     const bool update_cache);
static cache_obj_t *FIFO_Merge_insert(cache_t *cache, const request_t *req);
static cache_obj_t *FIFO_Merge_to_evict(cache_t *cache, const request_t *req);
static void FIFO_Merge_evict(cache_t *cache, const request_t *req);
static bool FIFO_Merge_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
static inline int cmp_list_node(const void *a0, const void *b0);
static inline double belady_metric(cache_t *cache, cache_obj_t *cache_obj);
static inline double freq_metric(cache_t *cache, cache_obj_t *cache_obj);
static inline double recency_metric(cache_t *cache, cache_obj_t *cache_obj);
static double retain_metric(cache_t *cache, cache_obj_t *cache_obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *FIFO_Merge_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("FIFO_Merge", ccache_params, cache_specific_params);
  cache->cache_init = FIFO_Merge_init;
  cache->cache_free = FIFO_Merge_free;
  cache->get = FIFO_Merge_get;
  cache->find = FIFO_Merge_find;
  cache->insert = FIFO_Merge_insert;
  cache->evict = FIFO_Merge_evict;
  cache->remove = FIFO_Merge_remove;
  cache->to_evict = FIFO_Merge_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 4;
  } else {
    cache->obj_md_size = 0;
  }

  FIFO_Merge_params_t *params = my_malloc(FIFO_Merge_params_t);
  memset(params, 0, sizeof(FIFO_Merge_params_t));
  cache->eviction_params = params;

  /* TODO: can we make this parameter adaptive to trace? */
  params->n_exam_obj = 100;
  params->n_keep_obj = params->n_exam_obj / 2;
  params->retain_policy = RETAIN_POLICY_FREQUENCY;
  params->next_to_exam = NULL;
  params->pos_in_metric_list = INT32_MAX;

  if (cache_specific_params != NULL) {
    FIFO_Merge_parse_params(cache, cache_specific_params);
  }

  assert(params->n_exam_obj > 0 && params->n_keep_obj >= 0);
  assert(params->n_keep_obj <= params->n_exam_obj);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "FIFO_Merge_%s",
           retain_policy_names[params->retain_policy]);
  params->metric_list = my_malloc_n(struct sort_list_node, params->n_exam_obj);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void FIFO_Merge_free(cache_t *cache) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;
  my_free(sizeof(struct sort_list_node) * params->n_exam_obj,
          params->metric_list);
  my_free(sizeof(FIFO_Merge_params_t), params);
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
static bool FIFO_Merge_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *FIFO_Merge_find(cache_t *cache, const request_t *req,
                                     const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && update_cache) {
    cache_obj->FIFO_Merge.freq++;

    cache_obj->FIFO_Merge.last_access_vtime = cache->n_req;
    cache_obj->misc.next_access_vtime = req->next_access_vtime;
  }

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
static cache_obj_t *FIFO_Merge_insert(cache_t *cache, const request_t *req) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  cache_obj->FIFO_Merge.freq = 0;
  cache_obj->FIFO_Merge.last_access_vtime = cache->n_req;
  cache_obj->misc.next_access_vtime = req->next_access_vtime;

  return cache_obj;
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
static cache_obj_t *FIFO_Merge_to_evict(cache_t *cache, const request_t *req) {
  ERROR("Undefined! Multiple objs will be evicted\n");
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
static void FIFO_Merge_evict(cache_t *cache, const request_t *req) {
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj = NULL;
  // the logic is that - we search for n_exam objects and identify n_exam -
  // n_keep objects to evict, each time we evict one object
  if (params->pos_in_metric_list < params->n_exam_obj) {
    cache_obj = params->metric_list[params->pos_in_metric_list++].cache_obj;
    if (params->pos_in_metric_list < params->n_exam_obj - params->n_keep_obj) {
      // there are objects identified to evict
      remove_obj_from_list(&params->q_head, &params->q_tail, cache_obj);
      cache_evict_base(cache, cache_obj, true);
      return;
    }
  }

  if (cache->n_obj <= params->n_exam_obj) {
    // just evict one object - this is fifo
    cache_obj = params->q_tail;
    params->next_to_exam = NULL;
    remove_obj_from_list(&params->q_head, &params->q_tail, cache_obj);
    cache_evict_base(cache, cache_obj, true);

    return;
  }

  // we need to exam n_exam objects
  // collect metric for n_exam obj, we will keep objects with larger metric
  int n_loop = 0;
  cache_obj = params->next_to_exam;
  for (int i = 0; i < params->n_exam_obj; i++) {
    if (cache_obj == NULL) {
      cache_obj = params->q_tail;
      n_loop += 1;
      assert(n_loop++ <= 2);
    }

    params->metric_list[i].metric = retain_metric(cache, cache_obj);
    params->metric_list[i].cache_obj = cache_obj;
    cache_obj = cache_obj->queue.prev;
  }
  params->next_to_exam = cache_obj;
  cache_obj = NULL;

  // sort metrics
  qsort(params->metric_list, params->n_exam_obj, sizeof(struct sort_list_node),
        cmp_list_node);

  // remove objects
  params->pos_in_metric_list = 1;
  cache_obj = params->metric_list[0].cache_obj;
  remove_obj_from_list(&params->q_head, &params->q_tail, cache_obj);
  cache_evict_base(cache, cache_obj, true);
}

static void FIFO_Merge_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  assert(obj_to_remove != NULL);
  FIFO_Merge_params_t *params = (FIFO_Merge_params_t *)cache->eviction_params;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
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
bool FIFO_Merge_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  FIFO_Merge_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
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
        params->n_keep_obj = params->n_exam_obj;
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

// ***********************************************************************
// ****                                                               ****
// ****                  cache internal functions                     ****
// ****                                                               ****
// ***********************************************************************
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

static inline double belady_metric(cache_t *cache, cache_obj_t *cache_obj) {
  if (cache_obj->misc.next_access_vtime == -1 ||
      cache_obj->misc.next_access_vtime == INT64_MAX)
    return -1;
  return 1.0e12 / (cache_obj->misc.next_access_vtime - cache->n_req) /
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
  abort();
  return -1;
}

#ifdef __cplusplus
}
#endif

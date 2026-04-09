//
//  ClockRI (Clock with fixed Reinsertion ratio)
//
//  A CLOCK/FIFO variant that examines a batch of objects at eviction time
//  and reinserts the most recently accessed x% (default 28%) to the head,
//  evicting the rest.
//
//  ClockRI.c
//  libCacheSim
//

#include <assert.h>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sort_list_node {
  int32_t last_access_vtime;
  cache_obj_t *cache_obj;
};

static const char *DEFAULT_PARAMS =
    "n-exam=100,reinsertion-ratio=0.28";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void ClockRI_parse_params(cache_t *cache,
                                 const char *cache_specific_params);
static void ClockRI_free(cache_t *cache);
static bool ClockRI_get(cache_t *cache, const request_t *req);
static cache_obj_t *ClockRI_find(cache_t *cache, const request_t *req,
                                 bool update_cache);
static cache_obj_t *ClockRI_insert(cache_t *cache, const request_t *req);
static cache_obj_t *ClockRI_to_evict(cache_t *cache, const request_t *req);
static void ClockRI_evict(cache_t *cache, const request_t *req);
static void ClockRI_remove_obj(cache_t *cache, cache_obj_t *obj);
static bool ClockRI_remove(cache_t *cache, obj_id_t obj_id);

static inline int cmp_list_node(const void *a0, const void *b0);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *ClockRI_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("ClockRI", ccache_params, cache_specific_params);
  cache->cache_init = ClockRI_init;
  cache->cache_free = ClockRI_free;
  cache->get = ClockRI_get;
  cache->find = ClockRI_find;
  cache->insert = ClockRI_insert;
  cache->evict = ClockRI_evict;
  cache->remove = ClockRI_remove;
  cache->to_evict = ClockRI_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = sizeof(ClockRI_obj_metadata_t);
  } else {
    cache->obj_md_size = 0;
  }

  ClockRI_params_t *params = my_malloc(ClockRI_params_t);
  memset(params, 0, sizeof(ClockRI_params_t));
  cache->eviction_params = params;

  params->q_head = NULL;
  params->q_tail = NULL;
  params->next_to_merge = NULL;
  params->n_exam_obj = 100;
  params->reinsertion_ratio = 0.28;

  if (cache_specific_params != NULL) {
    ClockRI_parse_params(cache, cache_specific_params);
  }

  assert(params->n_exam_obj > 0);
  assert(params->reinsertion_ratio >= 0.0 && params->reinsertion_ratio < 1.0);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "ClockRI-%.2lf-%d",
           params->reinsertion_ratio, params->n_exam_obj);

  return cache;
}

static void ClockRI_free(cache_t *cache) {
  my_free(sizeof(ClockRI_params_t), cache->eviction_params);
  cache_struct_free(cache);
}

static bool ClockRI_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *ClockRI_find(cache_t *cache, const request_t *req,
                                 bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && update_cache) {
    cache_obj->ClockRI.last_access_vtime = (int32_t)cache->n_req;
  }

  return cache_obj;
}

static cache_obj_t *ClockRI_insert(cache_t *cache, const request_t *req) {
  ClockRI_params_t *params = (ClockRI_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  obj->ClockRI.last_access_vtime = (int32_t)cache->n_req;

  return obj;
}

static cache_obj_t *ClockRI_to_evict(cache_t *cache, const request_t *req) {
  /* multiple objects may be evicted per call, cannot return a single one */
  ERROR("Undefined! Multiple objs will be evicted\n");
  abort();
  return NULL;
}

/**
 * @brief evict objects from the cache
 * examine n_exam_obj objects from the eviction end, reinsert the most recently
 * accessed reinsertion_ratio fraction to the head, evict the rest
 */
static void ClockRI_evict(cache_t *cache, const request_t *req) {
  ClockRI_params_t *params = (ClockRI_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = params->next_to_merge;
  int n_loop = 0;
  if (cache_obj == NULL) {
    params->next_to_merge = params->q_tail;
    cache_obj = params->q_tail;
    n_loop = 1;
  }

  /* if cache is too small, just evict one object */
  if (cache->n_obj <= params->n_exam_obj) {
    cache_obj_t *prev = params->next_to_merge->queue.prev;
    ClockRI_remove_obj(cache, params->next_to_merge);
    params->next_to_merge = prev;
    return;
  }

  int n_exam = params->n_exam_obj;
  int n_keep = (int)(n_exam * params->reinsertion_ratio);

  /* collect objects to examine */
  struct sort_list_node *metric_list =
      (struct sort_list_node *)alloca(sizeof(struct sort_list_node) * n_exam);

  for (int i = 0; i < n_exam; i++) {
    assert(cache_obj != NULL);
    metric_list[i].last_access_vtime = cache_obj->ClockRI.last_access_vtime;
    metric_list[i].cache_obj = cache_obj;
    cache_obj = cache_obj->queue.prev;

    if (cache_obj == NULL) {
      cache_obj = params->q_tail;
      DEBUG_ASSERT(n_loop++ <= 2);
    }
  }
  params->next_to_merge = cache_obj;

  /* sort by last_access_vtime ascending: smallest (oldest) first */
  qsort(metric_list, n_exam, sizeof(struct sort_list_node), cmp_list_node);

  /* evict the oldest (1 - reinsertion_ratio) fraction */
  int n_evict = n_exam - n_keep;
  for (int i = 0; i < n_evict; i++) {
    ClockRI_remove_obj(cache, metric_list[i].cache_obj);
  }

  /* reinsert the most recent n_keep objects to the head */
  for (int i = n_evict; i < n_exam; i++) {
    cache_obj_t *obj = metric_list[i].cache_obj;
    move_obj_to_head(&params->q_head, &params->q_tail, obj);

    params->n_obj_rewritten += 1;
    params->n_byte_rewritten += obj->obj_size;
  }
}

static void ClockRI_remove_obj(cache_t *cache, cache_obj_t *obj) {
  DEBUG_ASSERT(obj != NULL);
  ClockRI_params_t *params = (ClockRI_params_t *)cache->eviction_params;

  if (obj == params->next_to_merge) {
    params->next_to_merge = obj->queue.prev;
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool ClockRI_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  ClockRI_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *ClockRI_current_params(ClockRI_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-exam=%d, reinsertion-ratio=%.4lf",
           params->n_exam_obj, params->reinsertion_ratio);
  return params_str;
}

static void ClockRI_parse_params(cache_t *cache,
                                 const char *cache_specific_params) {
  ClockRI_params_t *params = (ClockRI_params_t *)cache->eviction_params;

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "n-exam") == 0) {
      params->n_exam_obj = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "reinsertion-ratio") == 0) {
      params->reinsertion_ratio = strtod(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("%s parameters: %s\n", cache->cache_name,
             ClockRI_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, example parameters %s\n",
            cache->cache_name, key, ClockRI_current_params(params));
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
  const struct sort_list_node *a = (const struct sort_list_node *)a0;
  const struct sort_list_node *b = (const struct sort_list_node *)b0;

  if (a->last_access_vtime < b->last_access_vtime) {
    return -1;
  } else if (a->last_access_vtime > b->last_access_vtime) {
    return 1;
  } else {
    return 0;
  }
}

#ifdef __cplusplus
}
#endif

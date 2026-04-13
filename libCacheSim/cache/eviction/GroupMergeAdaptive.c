//
//  GroupMergeAdaptive: adaptive group-based eviction.
//  - E (number of groups examined) grows linearly from e-start (at the tail)
//    to e-max (at the head of the current scan pass).
//  - Near the tail (start of pass), eviction may skip the merge scan with
//    some probability and just do plain FIFO eviction (fast path).
//  - Retained objects stay at their original position (like GroupMerge).
//
//  GroupMergeAdaptive.c
//  libCacheSim
//

#include <assert.h>
#include <stdlib.h>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

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
} retain_policy_t;

static const char *retain_policy_names[] = {"RECENCY", "FREQUENCY", "BELADY"};

typedef struct GroupMergeAdaptive_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the next object to start scanning from
  cache_obj_t *next_to_exam;

  // group size in bytes
  int64_t group_size;
  // E range: starts at e_start (at tail), grows to e_max (at head)
  int e_start;
  int e_max;
  // max skip probability at tail (decays to 0 at head)
  double skip_prob_max;

  retain_policy_t retain_policy;

  // pass tracking: bytes advanced by merge scans in current pass
  int64_t pass_bytes_advanced;

  // batch eviction state
  struct sort_list_node *metric_list;
  int metric_list_capacity;
  int n_objs_in_batch;
  int n_objs_to_evict;
  int pos_in_metric_list;

  int64_t n_obj_inserted;
  int64_t n_byte_inserted;
  int64_t n_obj_retained;
  int64_t n_byte_retained;

  int64_t n_skip_groups;
  int64_t n_merge_scans;
} GroupMergeAdaptive_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void GroupMergeAdaptive_parse_params(cache_t *cache,
                                            const char *cache_specific_params);
static void GroupMergeAdaptive_free(cache_t *cache);
static bool GroupMergeAdaptive_get(cache_t *cache, const request_t *req);
static cache_obj_t *GroupMergeAdaptive_find(cache_t *cache,
                                            const request_t *req,
                                            bool update_cache);
static cache_obj_t *GroupMergeAdaptive_insert(cache_t *cache,
                                              const request_t *req);
static cache_obj_t *GroupMergeAdaptive_to_evict(cache_t *cache,
                                                const request_t *req);
static void GroupMergeAdaptive_evict(cache_t *cache, const request_t *req);
static bool GroupMergeAdaptive_remove(cache_t *cache, obj_id_t obj_id);
static void GroupMergeAdaptive_remove_obj(cache_t *cache, cache_obj_t *obj);

/* internal functions */
static inline int cmp_list_node(const void *a0, const void *b0);
static double retain_metric(cache_t *cache, cache_obj_t *cache_obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *GroupMergeAdaptive_init(const common_cache_params_t ccache_params,
                                 const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GroupMergeAdaptive", ccache_params,
                                     cache_specific_params);
  cache->cache_init = GroupMergeAdaptive_init;
  cache->cache_free = GroupMergeAdaptive_free;
  cache->get = GroupMergeAdaptive_get;
  cache->find = GroupMergeAdaptive_find;
  cache->insert = GroupMergeAdaptive_insert;
  cache->evict = GroupMergeAdaptive_evict;
  cache->remove = GroupMergeAdaptive_remove;
  cache->to_evict = GroupMergeAdaptive_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8;
  } else {
    cache->obj_md_size = 0;
  }

  GroupMergeAdaptive_params_t *params = my_malloc(GroupMergeAdaptive_params_t);
  memset(params, 0, sizeof(GroupMergeAdaptive_params_t));
  cache->eviction_params = params;

  // default group_size = cache_size / 1000
  params->group_size = (int64_t)ccache_params.cache_size / 1000;
  if (params->group_size < 1) params->group_size = 1;
  params->e_start = 2;
  params->e_max = 16;
  params->skip_prob_max = 0.8;
  params->retain_policy = RETAIN_POLICY_RECENCY;
  params->next_to_exam = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;
  params->pos_in_metric_list = INT32_MAX;
  params->pass_bytes_advanced = 0;

  params->metric_list_capacity = 256;
  params->metric_list =
      malloc(sizeof(struct sort_list_node) * params->metric_list_capacity);

  if (cache_specific_params != NULL) {
    GroupMergeAdaptive_parse_params(cache, cache_specific_params);
  }

  assert(params->group_size > 0);
  assert(params->e_start >= 1 && params->e_max >= params->e_start);
  assert(params->skip_prob_max >= 0.0 && params->skip_prob_max <= 1.0);

  // clamp group_size so E*group_size does not dwarf the cache; otherwise
  // each scan covers ~the whole cache while retaining everything, forcing
  // batched evictions of 1, which is O(N^2) per pass.
  int64_t max_group_size =
      (int64_t)ccache_params.cache_size / (int64_t)(params->e_max * 2);
  if (max_group_size < 1) max_group_size = 1;
  if (params->group_size > max_group_size) {
    WARN("GroupMergeAdaptive: group-size %ld too large for cache size %lu; "
         "clamping to %ld (cache_size / (2*e_max))\n",
         (long)params->group_size, (unsigned long)ccache_params.cache_size,
         (long)max_group_size);
    params->group_size = max_group_size;
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "GroupMergeAdaptive_gs%ld_E%d-%d_sp%.2f_%s",
           (long)params->group_size, params->e_start, params->e_max,
           params->skip_prob_max, retain_policy_names[params->retain_policy]);

  return cache;
}

static void GroupMergeAdaptive_free(cache_t *cache) {
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;
  double retain_ratio = params->n_byte_inserted > 0
      ? (double)params->n_byte_retained / (double)params->n_byte_inserted
      : 0.0;
  INFO(
      "%s: inserted %ld obj / %ld bytes, retained %ld obj / %ld bytes "
      "(retained/inserted byte ratio = %.4f), merge scans %ld, skip groups %ld\n",
      cache->cache_name, (long)params->n_obj_inserted,
      (long)params->n_byte_inserted, (long)params->n_obj_retained,
      (long)params->n_byte_retained, retain_ratio,
      (long)params->n_merge_scans, (long)params->n_skip_groups);
  free(params->metric_list);
  my_free(sizeof(GroupMergeAdaptive_params_t), params);
  cache_struct_free(cache);
}

static bool GroupMergeAdaptive_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *GroupMergeAdaptive_find(cache_t *cache,
                                            const request_t *req,
                                            bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && update_cache) {
    cache_obj->GroupMerge.freq += 1;
    cache_obj->GroupMerge.last_access_vtime = (int32_t)cache->n_req;
  }

  return cache_obj;
}

static cache_obj_t *GroupMergeAdaptive_insert(cache_t *cache,
                                              const request_t *req) {
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  cache_obj->GroupMerge.freq = 0;
  cache_obj->GroupMerge.last_access_vtime = (int32_t)cache->n_req;

  params->n_obj_inserted += 1;
  params->n_byte_inserted += cache_obj->obj_size;

  return cache_obj;
}

static cache_obj_t *GroupMergeAdaptive_to_evict(cache_t *cache,
                                                const request_t *req) {
  ERROR("Undefined! Multiple objs will be evicted\n");
  abort();
  return NULL;
}

static inline double rand_unit(void) {
  return (double)(next_rand() % 100000) / 100000.0;
}

// Batch-retention bookkeeping shared by the "last-eviction-in-batch" paths.
static inline void finish_batch(GroupMergeAdaptive_params_t *params) {
  for (int i = params->n_objs_to_evict; i < params->n_objs_in_batch; i++) {
    cache_obj_t *retained = params->metric_list[i].cache_obj;
    retained->GroupMerge.freq = (retained->GroupMerge.freq + 1) / 2;
    params->n_obj_retained += 1;
    params->n_byte_retained += retained->obj_size;
  }
  params->pos_in_metric_list = INT32_MAX;
}

static void GroupMergeAdaptive_evict(cache_t *cache, const request_t *req) {
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;

  // drain pending batch evictions from a previous scan first
  if (params->pos_in_metric_list < params->n_objs_to_evict) {
    cache_obj_t *obj =
        params->metric_list[params->pos_in_metric_list++].cache_obj;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);

    if (params->pos_in_metric_list >= params->n_objs_to_evict) {
      finish_batch(params);
    }
    return;
  }

  // too few objects: plain FIFO
  if (cache->n_obj <= 1) {
    cache_obj_t *obj = params->q_tail;
    if (obj == NULL) return;
    if (obj == params->next_to_exam) params->next_to_exam = NULL;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    return;
  }

  cache_obj_t *scan = params->next_to_exam;
  if (scan == NULL) {
    // starting a new pass
    scan = params->q_tail;
    params->pass_bytes_advanced = 0;
  }

  cache_obj_t *start_obj = scan;
  bool wrapped = false;

  // skip tail groups with some probability. "Skip" means advance the cursor
  // past one group of bytes WITHOUT evicting anything, then re-decide from
  // the new (closer-to-head) position. As position grows, skip_prob drops,
  // so eventually we stop skipping and fall through to the merge scan.
  int max_skips = params->e_max * 4;  // safety bound
  for (int s = 0; s < max_skips; s++) {
    double position = 0.0;
    if (cache->cache_size > 0) {
      position = (double)params->pass_bytes_advanced /
                 (double)cache->cache_size;
      if (position > 1.0) position = 1.0;
    }
    double skip_prob = params->skip_prob_max * (1.0 - position);
    if (skip_prob <= 0.0 || rand_unit() >= skip_prob) break;

    // skip one group: advance cursor by group_size bytes
    int64_t skipped = 0;
    while (skipped < params->group_size && scan != NULL) {
      skipped += scan->obj_size;
      scan = scan->queue.prev;
      if (scan == NULL) {
        scan = params->q_tail;
        wrapped = true;
      }
      if (wrapped && scan == start_obj) break;
    }
    if (wrapped) {
      params->pass_bytes_advanced = 0;
    } else {
      params->pass_bytes_advanced += skipped;
    }
    params->n_skip_groups += 1;
    if (wrapped && scan == start_obj) break;
  }

  // recompute position for the merge E-ramp (may have grown from skipping)
  double position = 0.0;
  if (cache->cache_size > 0) {
    position = (double)params->pass_bytes_advanced / (double)cache->cache_size;
    if (position > 1.0) position = 1.0;
  }

  // decide how many groups to examine: linear ramp from e_start -> e_max
  int n_exam_groups =
      params->e_start + (int)((params->e_max - params->e_start) * position);
  if (n_exam_groups < 1) n_exam_groups = 1;

  int64_t target_bytes = (int64_t)n_exam_groups * params->group_size;
  int64_t scanned_bytes = 0;
  int n_objs = 0;

  while (scanned_bytes < target_bytes && scan != NULL) {
    if (n_objs >= params->metric_list_capacity) {
      params->metric_list_capacity *= 2;
      params->metric_list =
          realloc(params->metric_list,
                  sizeof(struct sort_list_node) * params->metric_list_capacity);
    }

    params->metric_list[n_objs].metric = retain_metric(cache, scan);
    params->metric_list[n_objs].cache_obj = scan;
    scanned_bytes += scan->obj_size;
    n_objs++;

    scan = scan->queue.prev;
    if (scan == NULL) {
      // reached head: wrap to tail and reset pass tracker
      scan = params->q_tail;
      wrapped = true;
    }
    if (wrapped && scan == start_obj) break;
  }
  params->next_to_exam = scan;
  params->n_merge_scans += 1;

  // advance the pass-position tracker; if we wrapped, restart the pass
  if (wrapped) {
    params->pass_bytes_advanced = 0;
  } else {
    params->pass_bytes_advanced += scanned_bytes;
  }

  if (n_objs <= 1) {
    cache_obj_t *obj = params->metric_list[0].cache_obj;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    params->pos_in_metric_list = INT32_MAX;
    return;
  }

  // sort ascending (lowest metric = evict first)
  qsort(params->metric_list, n_objs, sizeof(struct sort_list_node),
        cmp_list_node);

  // retain the best objects that fit in 1 group (group_size bytes)
  int64_t retained_bytes = 0;
  int n_retain = 0;
  for (int i = n_objs - 1; i >= 0; i--) {
    int64_t obj_size = params->metric_list[i].cache_obj->obj_size;
    if (retained_bytes + obj_size <= params->group_size) {
      retained_bytes += obj_size;
      n_retain++;
    } else if (n_retain == 0) {
      retained_bytes += obj_size;
      n_retain++;
    } else {
      break;
    }
  }

  int n_evict = n_objs - n_retain;
  if (n_evict == 0) n_evict = 1;

  params->n_objs_in_batch = n_objs;
  params->n_objs_to_evict = n_evict;

  // evict the first (lowest metric)
  params->pos_in_metric_list = 1;
  cache_obj_t *obj = params->metric_list[0].cache_obj;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);

  if (params->pos_in_metric_list >= params->n_objs_to_evict) {
    finish_batch(params);
  }
}

static void GroupMergeAdaptive_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;

  if (obj == params->next_to_exam) {
    params->next_to_exam = obj->queue.prev;
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool GroupMergeAdaptive_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  GroupMergeAdaptive_remove_obj(cache, obj);
  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *GroupMergeAdaptive_current_params(
    GroupMergeAdaptive_params_t *params) {
  static __thread char params_str[192];
  snprintf(params_str, 192,
           "group-size=%ld, e-start=%d, e-max=%d, skip-prob-max=%.3f, "
           "retain-policy=%s",
           (long)params->group_size, params->e_start, params->e_max,
           params->skip_prob_max,
           retain_policy_names[params->retain_policy]);
  return params_str;
}

static void GroupMergeAdaptive_parse_params(cache_t *cache,
                                            const char *cache_specific_params) {
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "group-size") == 0) {
      params->group_size = (int64_t)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "e-start") == 0 ||
               strcasecmp(key, "n-exam") == 0) {
      params->e_start = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "e-max") == 0) {
      params->e_max = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "skip-prob-max") == 0 ||
               strcasecmp(key, "skip-prob") == 0) {
      params->skip_prob_max = strtod(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "retain-policy") == 0) {
      if (strcasecmp(value, "freq") == 0 ||
          strcasecmp(value, "frequency") == 0)
        params->retain_policy = RETAIN_POLICY_FREQUENCY;
      else if (strcasecmp(value, "recency") == 0)
        params->retain_policy = RETAIN_POLICY_RECENCY;
      else if (strcasecmp(value, "belady") == 0 ||
               strcasecmp(value, "optimal") == 0)
        params->retain_policy = RETAIN_POLICY_BELADY;
      else {
        ERROR("unknown retain-policy %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("%s parameters: %s\n", cache->cache_name,
             GroupMergeAdaptive_current_params(params));
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

  if (a->metric > b->metric)
    return 1;
  else if (a->metric < b->metric)
    return -1;
  else
    return 0;
}

static inline double belady_metric(cache_t *cache, cache_obj_t *cache_obj) {
  if (cache_obj->next_access_vtime == -1 ||
      cache_obj->next_access_vtime == INT64_MAX)
    return -1;
  return 1.0e12 / (cache_obj->next_access_vtime - cache->n_req) /
         (double)cache_obj->obj_size;
}

static inline double freq_metric(cache_t *cache, cache_obj_t *cache_obj) {
  double r = (double)(next_rand() % 1000) / 10000.0;
  return 1.0e6 * ((double)cache_obj->GroupMerge.freq + r) /
         (double)cache_obj->obj_size;
}

static inline double recency_metric(cache_t *cache, cache_obj_t *cache_obj) {
  return 1.0e12 /
         (double)(cache->n_req - cache_obj->GroupMerge.last_access_vtime) /
         (double)cache_obj->obj_size;
}

static double retain_metric(cache_t *cache, cache_obj_t *cache_obj) {
  GroupMergeAdaptive_params_t *params =
      (GroupMergeAdaptive_params_t *)cache->eviction_params;

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

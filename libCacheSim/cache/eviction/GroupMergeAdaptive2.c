//
//  GroupMergeAdaptive2: activity-driven adaptive group-based eviction.
//  - Every object has a `seen` bit set on access; the scanner clears it when
//    it passes over the object.
//  - Per group (group_size bytes at the tail / next_to_exam cursor), we
//    measure the fraction of bytes whose `seen` bit is set (= "active").
//  - If the current head group is more than `active_threshold` (default 50%)
//    active, the scanner SKIPS it: the cursor advances past the group without
//    evicting and without clearing `seen` bits.
//  - Otherwise, the scanner merges this group into the working window and
//    keeps merging more groups until the cumulative active bytes across the
//    window is >= group_size (~100% of one group's worth of hot data), up to
//    an `e_max` safety cap. Then the parent GroupMerge sort/retain step keeps
//    one group worth of the highest-metric objects and evicts the rest.
//
//  GroupMergeAdaptive2.c
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

typedef struct GroupMergeAdaptive2_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the next object to start scanning from
  cache_obj_t *next_to_exam;

  // group size in bytes
  int64_t group_size;
  // safety cap on merge-window width (in groups)
  int e_max;
  // a group with active_bytes / total_bytes > active_threshold is skipped
  double active_threshold;

  retain_policy_t retain_policy;

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
  int64_t n_merge_groups_total;
} GroupMergeAdaptive2_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void GroupMergeAdaptive2_parse_params(
    cache_t *cache, const char *cache_specific_params);
static void GroupMergeAdaptive2_free(cache_t *cache);
static bool GroupMergeAdaptive2_get(cache_t *cache, const request_t *req);
static cache_obj_t *GroupMergeAdaptive2_find(cache_t *cache,
                                             const request_t *req,
                                             bool update_cache);
static cache_obj_t *GroupMergeAdaptive2_insert(cache_t *cache,
                                               const request_t *req);
static cache_obj_t *GroupMergeAdaptive2_to_evict(cache_t *cache,
                                                 const request_t *req);
static void GroupMergeAdaptive2_evict(cache_t *cache, const request_t *req);
static bool GroupMergeAdaptive2_remove(cache_t *cache, obj_id_t obj_id);
static void GroupMergeAdaptive2_remove_obj(cache_t *cache, cache_obj_t *obj);

/* internal functions */
static inline int cmp_list_node(const void *a0, const void *b0);
static double retain_metric(cache_t *cache, cache_obj_t *cache_obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *GroupMergeAdaptive2_init(const common_cache_params_t ccache_params,
                                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GroupMergeAdaptive2", ccache_params,
                                     cache_specific_params);
  cache->cache_init = GroupMergeAdaptive2_init;
  cache->cache_free = GroupMergeAdaptive2_free;
  cache->get = GroupMergeAdaptive2_get;
  cache->find = GroupMergeAdaptive2_find;
  cache->insert = GroupMergeAdaptive2_insert;
  cache->evict = GroupMergeAdaptive2_evict;
  cache->remove = GroupMergeAdaptive2_remove;
  cache->to_evict = GroupMergeAdaptive2_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 9;  // GroupMergeAdaptive2_obj_metadata_t
  } else {
    cache->obj_md_size = 0;
  }

  GroupMergeAdaptive2_params_t *params =
      my_malloc(GroupMergeAdaptive2_params_t);
  memset(params, 0, sizeof(GroupMergeAdaptive2_params_t));
  cache->eviction_params = params;

  // defaults
  params->group_size = (int64_t)ccache_params.cache_size / 1000;
  if (params->group_size < 1) params->group_size = 1;
  params->e_max = 32;
  params->active_threshold = 0.5;
  params->retain_policy = RETAIN_POLICY_RECENCY;
  params->next_to_exam = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;
  params->pos_in_metric_list = INT32_MAX;

  params->metric_list_capacity = 256;
  params->metric_list =
      malloc(sizeof(struct sort_list_node) * params->metric_list_capacity);

  if (cache_specific_params != NULL) {
    GroupMergeAdaptive2_parse_params(cache, cache_specific_params);
  }

  assert(params->group_size > 0);
  assert(params->e_max >= 1);
  assert(params->active_threshold >= 0.0 && params->active_threshold <= 1.0);

  // clamp group_size to avoid pathological E*group_size >> cache_size
  int64_t max_group_size =
      (int64_t)ccache_params.cache_size / (int64_t)(params->e_max * 2);
  if (max_group_size < 1) max_group_size = 1;
  if (params->group_size > max_group_size) {
    WARN("GroupMergeAdaptive2: group-size %ld too large for cache size %lu; "
         "clamping to %ld (cache_size / (2*e_max))\n",
         (long)params->group_size, (unsigned long)ccache_params.cache_size,
         (long)max_group_size);
    params->group_size = max_group_size;
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "GroupMergeAdaptive2_gs%ld_emax%d_th%.2f_%s",
           (long)params->group_size, params->e_max, params->active_threshold,
           retain_policy_names[params->retain_policy]);

  return cache;
}

static void GroupMergeAdaptive2_free(cache_t *cache) {
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;
  double retain_ratio = params->n_byte_inserted > 0
      ? (double)params->n_byte_retained / (double)params->n_byte_inserted
      : 0.0;
  double avg_merge_groups = params->n_merge_scans > 0
      ? (double)params->n_merge_groups_total / (double)params->n_merge_scans
      : 0.0;
  INFO(
      "%s: inserted %ld obj / %ld bytes, retained %ld obj / %ld bytes "
      "(retained/inserted byte ratio = %.4f), merge scans %ld "
      "(avg groups/scan = %.2f), skip groups %ld\n",
      cache->cache_name, (long)params->n_obj_inserted,
      (long)params->n_byte_inserted, (long)params->n_obj_retained,
      (long)params->n_byte_retained, retain_ratio,
      (long)params->n_merge_scans, avg_merge_groups,
      (long)params->n_skip_groups);
  free(params->metric_list);
  my_free(sizeof(GroupMergeAdaptive2_params_t), params);
  cache_struct_free(cache);
}

static bool GroupMergeAdaptive2_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *GroupMergeAdaptive2_find(cache_t *cache,
                                             const request_t *req,
                                             bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && update_cache) {
    cache_obj->GroupMergeAdaptive2.freq += 1;
    cache_obj->GroupMergeAdaptive2.last_access_vtime = (int32_t)cache->n_req;
    cache_obj->GroupMergeAdaptive2.seen = 1;
  }

  return cache_obj;
}

static cache_obj_t *GroupMergeAdaptive2_insert(cache_t *cache,
                                               const request_t *req) {
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  cache_obj->GroupMergeAdaptive2.freq = 0;
  cache_obj->GroupMergeAdaptive2.last_access_vtime = (int32_t)cache->n_req;
  cache_obj->GroupMergeAdaptive2.seen = 0;

  params->n_obj_inserted += 1;
  params->n_byte_inserted += cache_obj->obj_size;

  return cache_obj;
}

static cache_obj_t *GroupMergeAdaptive2_to_evict(cache_t *cache,
                                                 const request_t *req) {
  ERROR("Undefined! Multiple objs will be evicted\n");
  abort();
  return NULL;
}

// Batch-retention bookkeeping: for objects that were scanned but not evicted,
// decay the long-term freq counter (consistent with other GroupMerge variants).
static inline void finish_batch(GroupMergeAdaptive2_params_t *params) {
  for (int i = params->n_objs_to_evict; i < params->n_objs_in_batch; i++) {
    cache_obj_t *retained = params->metric_list[i].cache_obj;
    retained->GroupMergeAdaptive2.freq =
        (retained->GroupMergeAdaptive2.freq + 1) / 2;
    params->n_obj_retained += 1;
    params->n_byte_retained += retained->obj_size;
  }
  params->pos_in_metric_list = INT32_MAX;
}

static inline void ensure_metric_list_capacity(
    GroupMergeAdaptive2_params_t *params, int need) {
  if (need <= params->metric_list_capacity) return;
  while (params->metric_list_capacity < need) {
    params->metric_list_capacity *= 2;
  }
  params->metric_list = realloc(
      params->metric_list,
      sizeof(struct sort_list_node) * params->metric_list_capacity);
}

static void GroupMergeAdaptive2_evict(cache_t *cache, const request_t *req) {
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;

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
  if (scan == NULL) scan = params->q_tail;

  cache_obj_t *start_obj = scan;
  bool wrapped = false;

  int64_t cum_active_bytes = 0;
  int n_objs = 0;
  int groups_merged = 0;
  int skip_guard = params->e_max * 4;  // absolute cap on skip attempts

  // Walk up to e_max groups. For each, tentatively append its objects to
  // metric_list and compute per-group active ratio. If the very first group
  // (nothing committed yet) is too active, roll back and skip past it.
  // Otherwise keep the group and continue merging until cum_active_bytes
  // reaches one group_size worth.
  while (scan != NULL && groups_merged < params->e_max) {
    int n_objs_before_group = n_objs;
    int64_t active_before_group = cum_active_bytes;

    int64_t this_group_bytes = 0;
    int64_t this_group_active = 0;

    while (this_group_bytes < params->group_size && scan != NULL) {
      ensure_metric_list_capacity(params, n_objs + 1);
      params->metric_list[n_objs].metric = retain_metric(cache, scan);
      params->metric_list[n_objs].cache_obj = scan;

      this_group_bytes += scan->obj_size;
      if (scan->GroupMergeAdaptive2.seen) {
        this_group_active += scan->obj_size;
        cum_active_bytes += scan->obj_size;
      }
      n_objs++;

      scan = scan->queue.prev;
      if (scan == NULL) {
        scan = params->q_tail;
        wrapped = true;
      }
      if (wrapped && scan == start_obj) break;
    }

    double ratio = this_group_bytes > 0
        ? (double)this_group_active / (double)this_group_bytes
        : 0.0;

    // Skip logic: only when no group has been committed yet and this one is
    // too hot. "Skip" = rollback (don't clear seen bits, don't evict from it),
    // advance cursor past the group, try again.
    if (n_objs_before_group == 0 && ratio > params->active_threshold &&
        skip_guard > 0) {
      n_objs = n_objs_before_group;
      cum_active_bytes = active_before_group;
      params->n_skip_groups += 1;
      skip_guard--;
      // reset the wrap-detection anchor to the new position so one full loop
      // around the ring still terminates
      start_obj = scan;
      wrapped = false;
      if (scan == NULL) break;
      continue;
    }

    // Commit: clear `seen` on all objects from this group now that we're
    // keeping the group in the merge window.
    for (int i = n_objs_before_group; i < n_objs; i++) {
      params->metric_list[i].cache_obj->GroupMergeAdaptive2.seen = 0;
    }
    groups_merged += 1;

    if (cum_active_bytes >= params->group_size) break;
    if (wrapped && scan == start_obj) break;
  }

  params->next_to_exam = scan;
  params->n_merge_scans += 1;
  params->n_merge_groups_total += groups_merged;

  // Degenerate: nothing committed (e.g. scan was NULL at entry). Fall back
  // to plain FIFO eviction from the tail so we make progress.
  if (n_objs == 0) {
    cache_obj_t *obj = params->q_tail;
    if (obj == NULL) return;
    if (obj == params->next_to_exam) params->next_to_exam = NULL;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    return;
  }

  if (n_objs == 1) {
    cache_obj_t *obj = params->metric_list[0].cache_obj;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    params->pos_in_metric_list = INT32_MAX;
    return;
  }

  // sort ascending (lowest metric = evict first)
  qsort(params->metric_list, n_objs, sizeof(struct sort_list_node),
        cmp_list_node);

  // retain the highest-metric objects that fit in 1 group_size of bytes
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

  // evict the first (lowest metric) now; remaining batch drains on future
  // evict() calls.
  params->pos_in_metric_list = 1;
  cache_obj_t *obj = params->metric_list[0].cache_obj;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);

  if (params->pos_in_metric_list >= params->n_objs_to_evict) {
    finish_batch(params);
  }
}

static void GroupMergeAdaptive2_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;

  if (obj == params->next_to_exam) {
    params->next_to_exam = obj->queue.prev;
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool GroupMergeAdaptive2_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  GroupMergeAdaptive2_remove_obj(cache, obj);
  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *GroupMergeAdaptive2_current_params(
    GroupMergeAdaptive2_params_t *params) {
  static __thread char params_str[192];
  snprintf(params_str, 192,
           "group-size=%ld, e-max=%d, active-threshold=%.3f, "
           "retain-policy=%s",
           (long)params->group_size, params->e_max, params->active_threshold,
           retain_policy_names[params->retain_policy]);
  return params_str;
}

static void GroupMergeAdaptive2_parse_params(
    cache_t *cache, const char *cache_specific_params) {
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;

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
    } else if (strcasecmp(key, "e-max") == 0 ||
               strcasecmp(key, "n-exam-max") == 0) {
      params->e_max = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "active-threshold") == 0 ||
               strcasecmp(key, "threshold") == 0) {
      params->active_threshold = strtod(value, &end);
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
             GroupMergeAdaptive2_current_params(params));
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
  return 1.0e6 * ((double)cache_obj->GroupMergeAdaptive2.freq + r) /
         (double)cache_obj->obj_size;
}

static inline double recency_metric(cache_t *cache, cache_obj_t *cache_obj) {
  return 1.0e12 /
         (double)(cache->n_req -
                  cache_obj->GroupMergeAdaptive2.last_access_vtime) /
         (double)cache_obj->obj_size;
}

static double retain_metric(cache_t *cache, cache_obj_t *cache_obj) {
  GroupMergeAdaptive2_params_t *params =
      (GroupMergeAdaptive2_params_t *)cache->eviction_params;

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

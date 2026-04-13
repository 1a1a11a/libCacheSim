//
//  GroupMergeHead: group-based eviction where groups are defined by byte size.
//  Each eviction examines E groups (E * group_size bytes) from the FIFO tail,
//  retains the best objects fitting in 1 group, evicts the rest.
//  Retained objects are reinserted at the head (most recently-inserted point).
//
//  GroupMergeHead.c
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

typedef struct GroupMergeHead_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the next object to start scanning from
  cache_obj_t *next_to_exam;

  // group size in bytes
  int64_t group_size;
  // number of groups to examine per eviction (E)
  int n_exam_groups;

  retain_policy_t retain_policy;

  // batch eviction state
  struct sort_list_node *metric_list;
  int metric_list_capacity;
  int n_objs_in_batch;
  int n_objs_to_evict;
  int pos_in_metric_list;

  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;
} GroupMergeHead_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void GroupMergeHead_parse_params(cache_t *cache,
                                        const char *cache_specific_params);
static void GroupMergeHead_free(cache_t *cache);
static bool GroupMergeHead_get(cache_t *cache, const request_t *req);
static cache_obj_t *GroupMergeHead_find(cache_t *cache, const request_t *req,
                                        bool update_cache);
static cache_obj_t *GroupMergeHead_insert(cache_t *cache, const request_t *req);
static cache_obj_t *GroupMergeHead_to_evict(cache_t *cache,
                                            const request_t *req);
static void GroupMergeHead_evict(cache_t *cache, const request_t *req);
static bool GroupMergeHead_remove(cache_t *cache, obj_id_t obj_id);
static void GroupMergeHead_remove_obj(cache_t *cache, cache_obj_t *obj);

/* internal functions */
static inline int cmp_list_node(const void *a0, const void *b0);
static double retain_metric(cache_t *cache, cache_obj_t *cache_obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *GroupMergeHead_init(const common_cache_params_t ccache_params,
                             const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("GroupMergeHead", ccache_params, cache_specific_params);
  cache->cache_init = GroupMergeHead_init;
  cache->cache_free = GroupMergeHead_free;
  cache->get = GroupMergeHead_get;
  cache->find = GroupMergeHead_find;
  cache->insert = GroupMergeHead_insert;
  cache->evict = GroupMergeHead_evict;
  cache->remove = GroupMergeHead_remove;
  cache->to_evict = GroupMergeHead_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8;
  } else {
    cache->obj_md_size = 0;
  }

  GroupMergeHead_params_t *params = my_malloc(GroupMergeHead_params_t);
  memset(params, 0, sizeof(GroupMergeHead_params_t));
  cache->eviction_params = params;

  params->group_size = 20 * 1024 * 1024;  // 20 MiB
  params->n_exam_groups = 4;
  params->retain_policy = RETAIN_POLICY_RECENCY;
  params->next_to_exam = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;
  params->pos_in_metric_list = INT32_MAX;

  params->metric_list_capacity = 256;
  params->metric_list =
      malloc(sizeof(struct sort_list_node) * params->metric_list_capacity);

  if (cache_specific_params != NULL) {
    GroupMergeHead_parse_params(cache, cache_specific_params);
  }

  assert(params->group_size > 0 && params->n_exam_groups >= 2);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "GroupMergeHead_gs%ld_E%d_%s", (long)params->group_size,
           params->n_exam_groups,
           retain_policy_names[params->retain_policy]);

  return cache;
}

static void GroupMergeHead_free(cache_t *cache) {
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;
  free(params->metric_list);
  my_free(sizeof(GroupMergeHead_params_t), params);
  cache_struct_free(cache);
}

static bool GroupMergeHead_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *GroupMergeHead_find(cache_t *cache, const request_t *req,
                                        bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && update_cache) {
    cache_obj->GroupMerge.freq += 1;
    cache_obj->GroupMerge.last_access_vtime = (int32_t)cache->n_req;
  }

  return cache_obj;
}

static cache_obj_t *GroupMergeHead_insert(cache_t *cache,
                                          const request_t *req) {
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  cache_obj->GroupMerge.freq = 0;
  cache_obj->GroupMerge.last_access_vtime = (int32_t)cache->n_req;

  return cache_obj;
}

static cache_obj_t *GroupMergeHead_to_evict(cache_t *cache,
                                            const request_t *req) {
  ERROR("Undefined! Multiple objs will be evicted\n");
  abort();
  return NULL;
}

static void GroupMergeHead_evict(cache_t *cache, const request_t *req) {
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;

  // check if we have pending evictions from a previous scan
  if (params->pos_in_metric_list < params->n_objs_to_evict) {
    cache_obj_t *obj =
        params->metric_list[params->pos_in_metric_list++].cache_obj;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);

    // after the last eviction in the batch, reinsert retained objects at head
    if (params->pos_in_metric_list >= params->n_objs_to_evict) {
      for (int i = params->n_objs_to_evict; i < params->n_objs_in_batch; i++) {
        cache_obj_t *retained = params->metric_list[i].cache_obj;
        move_obj_to_head(&params->q_head, &params->q_tail, retained);
        retained->GroupMerge.freq = (retained->GroupMerge.freq + 1) / 2;

        params->n_obj_rewritten += 1;
        params->n_byte_rewritten += retained->obj_size;
      }
      params->pos_in_metric_list = INT32_MAX;
    }
    return;
  }

  // if very few objects, just do FIFO
  if (cache->n_obj <= 1) {
    cache_obj_t *obj = params->q_tail;
    if (obj == NULL) return;
    params->next_to_exam = NULL;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    return;
  }

  // scan E groups worth of bytes from the tail
  int64_t target_bytes = (int64_t)params->n_exam_groups * params->group_size;
  int64_t scanned_bytes = 0;
  int n_objs = 0;

  cache_obj_t *scan = params->next_to_exam;
  if (scan == NULL) scan = params->q_tail;

  cache_obj_t *start_obj = scan;
  bool wrapped = false;

  while (scanned_bytes < target_bytes && scan != NULL) {
    // ensure metric_list capacity
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
      scan = params->q_tail;
      wrapped = true;
    }
    if (wrapped && scan == start_obj) break;
  }
  params->next_to_exam = scan;

  if (n_objs <= 1) {
    cache_obj_t *obj = params->metric_list[0].cache_obj;
    remove_obj_from_list(&params->q_head, &params->q_tail, obj);
    cache_evict_base(cache, obj, true);
    params->pos_in_metric_list = INT32_MAX;
    return;
  }

  // sort by metric ascending (lowest metric = evict first)
  qsort(params->metric_list, n_objs, sizeof(struct sort_list_node),
        cmp_list_node);

  // determine how many to retain: fill 1 group (group_size bytes)
  int64_t retained_bytes = 0;
  int n_retain = 0;
  for (int i = n_objs - 1; i >= 0; i--) {
    int64_t obj_size = params->metric_list[i].cache_obj->obj_size;
    if (retained_bytes + obj_size <= params->group_size) {
      retained_bytes += obj_size;
      n_retain++;
    } else if (n_retain == 0) {
      // always keep at least one object even if it exceeds group_size
      retained_bytes += obj_size;
      n_retain++;
    } else {
      break;
    }
  }

  int n_evict = n_objs - n_retain;
  if (n_evict == 0) n_evict = 1;  // always evict at least 1

  params->n_objs_in_batch = n_objs;
  params->n_objs_to_evict = n_evict;

  // evict the first object (lowest metric)
  params->pos_in_metric_list = 1;
  cache_obj_t *obj = params->metric_list[0].cache_obj;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);

  // if only one object to evict, reinsert retained now
  if (params->pos_in_metric_list >= params->n_objs_to_evict) {
    for (int i = params->n_objs_to_evict; i < params->n_objs_in_batch; i++) {
      cache_obj_t *retained = params->metric_list[i].cache_obj;
      move_obj_to_head(&params->q_head, &params->q_tail, retained);
      retained->GroupMerge.freq = (retained->GroupMerge.freq + 1) / 2;

      params->n_obj_rewritten += 1;
      params->n_byte_rewritten += retained->obj_size;
    }
    params->pos_in_metric_list = INT32_MAX;
  }
}

static void GroupMergeHead_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;

  if (obj == params->next_to_exam) {
    params->next_to_exam = obj->queue.prev;
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool GroupMergeHead_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  GroupMergeHead_remove_obj(cache, obj);
  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *GroupMergeHead_current_params(
    GroupMergeHead_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128,
           "group-size=%ld, n-exam=%d, retain-policy=%s",
           (long)params->group_size, params->n_exam_groups,
           retain_policy_names[params->retain_policy]);
  return params_str;
}

static void GroupMergeHead_parse_params(cache_t *cache,
                                        const char *cache_specific_params) {
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;

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
    } else if (strcasecmp(key, "n-exam") == 0) {
      params->n_exam_groups = (int)strtol(value, &end, 0);
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
             GroupMergeHead_current_params(params));
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
  GroupMergeHead_params_t *params =
      (GroupMergeHead_params_t *)cache->eviction_params;

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

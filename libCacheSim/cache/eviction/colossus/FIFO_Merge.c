/** FIFO merge of Colossus segments with byte-budgeted LRU block removal.
 *
 * This is the counterpart to colossus/FIFO_Reinsertion.c.
 * Instead of reinserting the survivors at the head, this keeps them in-place.
 */

#include "colossus_common.h"
#include "dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  colossus_segment_t *head_segment;  // newest, open for new admissions
  colossus_segment_t *tail_segment;  // oldest
  colossus_segment_t
      *next_to_merge;  // sweep position, NULL restarts at the tail
  int64_t n_segment;

  int64_t segment_size;
  double segment_size_ratio;
  double retain_ratio;

  // statistics: used to report the performance
  int64_t n_byte_rewritten;
  int64_t n_obj_rewritten;
  int64_t n_segment_merged;
  int64_t n_segment_evicted;
} Colossus_FIFO_Merge_params_t;

static const char *Colossus_FIFO_Merge_DEFAULT_CACHE_PARAMS =
    "segment-size-ratio=0.0625,retain-ratio=0.28";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Colossus_FIFO_Merge_free(cache_t *cache);
static bool Colossus_FIFO_Merge_get(cache_t *cache, const request_t *req);
static cache_obj_t *Colossus_FIFO_Merge_find(cache_t *cache,
                                             const request_t *req,
                                             bool update_cache);
static cache_obj_t *Colossus_FIFO_Merge_insert(cache_t *cache,
                                               const request_t *req);
static cache_obj_t *Colossus_FIFO_Merge_to_evict(cache_t *cache,
                                                 const request_t *req);
static void Colossus_FIFO_Merge_evict(cache_t *cache, const request_t *req);
static bool Colossus_FIFO_Merge_remove(cache_t *cache, obj_id_t obj_id);
static void Colossus_FIFO_Merge_print_cache(const cache_t *cache);
static void Colossus_FIFO_Merge_parse_params(cache_t *cache,
                                             const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   segment queue helpers                       ****
// ****                                                               ****
// ***********************************************************************

// allocate a new empty segment and push it onto the head of the queue
static colossus_segment_t *Colossus_FIFO_Merge_open_new_segment(
    Colossus_FIFO_Merge_params_t *params) {
  colossus_segment_t *new_segment = my_malloc(colossus_segment_t);
  memset(new_segment, 0, sizeof(colossus_segment_t));
  new_segment->prev = params->head_segment;
  new_segment->next = NULL;

  if (params->head_segment != NULL) {
    params->head_segment->next = new_segment;
  } else {
    params->tail_segment = new_segment;
  }
  params->head_segment = new_segment;
  params->n_segment++;

  return new_segment;
}

// unlink a segment from the queue and free it
static void Colossus_FIFO_Merge_unlink_segment(
    Colossus_FIFO_Merge_params_t *params, colossus_segment_t *segment) {
  // the sweep must never keep naming a segment that is about to be freed
  if (params->next_to_merge == segment) {
    params->next_to_merge = segment->next;
  }

  if (segment->prev != NULL) {
    segment->prev->next = segment->next;
  } else {
    params->tail_segment = segment->next;
  }

  if (segment->next != NULL) {
    segment->next->prev = segment->prev;
  } else {
    params->head_segment = segment->prev;
  }
  params->n_segment--;
  my_free(sizeof(colossus_segment_t), segment);
}

// pick the victim segment for the next eviction
// it wraps around to the tail when it reaches the head
// NOTE: this skip the newest head segment that is still open for new admissions
static colossus_segment_t *Colossus_FIFO_Merge_pick_victim_segment(
    Colossus_FIFO_Merge_params_t *params) {
  colossus_segment_t *victim = params->next_to_merge;
  if (victim == NULL) victim = params->tail_segment;
  if (victim == params->head_segment && victim != params->tail_segment) {
    victim = params->tail_segment;
  }
  return victim;
}

// remove obj from the segment holding it
static void Colossus_FIFO_Merge_detach_obj(colossus_segment_t *segment,
                                           cache_obj_t *obj) {
  remove_obj_from_list(&segment->q_head, &segment->q_tail, obj);
  segment->n_obj--;
  segment->n_byte -= obj->obj_size;
}

// insert obj into the segment (at the head)
static void Colossus_FIFO_Merge_attach_obj_head(colossus_segment_t *segment,
                                                cache_obj_t *obj) {
  prepend_obj_to_head(&segment->q_head, &segment->q_tail, obj);
  segment->n_obj++;
  segment->n_byte += obj->obj_size;
  obj->Colossus.segment = segment;
}

// check whether we need to create a new segment or not to accommodate a new
// object
static colossus_segment_t *Colossus_FIFO_Merge_check_head_segment(
    Colossus_FIFO_Merge_params_t *params, int64_t obj_size) {
  if (params->head_segment == NULL ||
      params->head_segment->n_byte + obj_size > params->segment_size) {
    if (params->head_segment == NULL || params->head_segment->n_byte > 0) {
      return Colossus_FIFO_Merge_open_new_segment(params);
    }
  }
  return params->head_segment;
}

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *Colossus_FIFO_Merge_init(const common_cache_params_t ccache_params,
                                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Colossus-FIFO_Merge", ccache_params,
                                     cache_specific_params);
  cache->cache_init = Colossus_FIFO_Merge_init;
  cache->cache_free = Colossus_FIFO_Merge_free;
  cache->get = Colossus_FIFO_Merge_get;
  cache->find = Colossus_FIFO_Merge_find;
  cache->insert = Colossus_FIFO_Merge_insert;
  cache->evict = Colossus_FIFO_Merge_evict;
  cache->remove = Colossus_FIFO_Merge_remove;
  cache->to_evict = Colossus_FIFO_Merge_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->print_cache = Colossus_FIFO_Merge_print_cache;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 32;  // two queue pointers + colossus metadata
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(Colossus_FIFO_Merge_params_t);
  memset(cache->eviction_params, 0, sizeof(Colossus_FIFO_Merge_params_t));
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;
  params->segment_size = -1;

  Colossus_FIFO_Merge_parse_params(cache,
                                   Colossus_FIFO_Merge_DEFAULT_CACHE_PARAMS);

  if (cache_specific_params != NULL) {
    Colossus_FIFO_Merge_parse_params(cache, cache_specific_params);
  }

  // explicit segment-size declared --> use it over the ratio
  if (params->segment_size <= 0) {
    params->segment_size =
        (int64_t)((double)cache->cache_size * params->segment_size_ratio);
  }
  if (params->segment_size <= 0) params->segment_size = 1;
  if (params->segment_size > cache->cache_size) {
    ERROR(
        "Colossus-FIFO_Merge: segment size (%lld) is larger than cache size "
        "(%lld)\n",
        (long long)params->segment_size, (long long)cache->cache_size);
    exit(1);
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "Colossus-FIFO_Merge-%lld-%.4lf", (long long)params->segment_size,
           params->retain_ratio);

  return cache;
}

static void Colossus_FIFO_Merge_free(cache_t *cache) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;
  colossus_segment_t *segment = params->tail_segment;
  while (segment != NULL) {
    colossus_segment_t *next = segment->next;
    my_free(sizeof(colossus_segment_t), segment);
    segment = next;
  }
  my_free(sizeof(Colossus_FIFO_Merge_params_t), params);
  cache_struct_free(cache);
}

static bool Colossus_FIFO_Merge_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *Colossus_FIFO_Merge_find(cache_t *cache,
                                             const request_t *req,
                                             bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj != NULL && update_cache) {
    // update the object's last access request
    obj->Colossus.last_access_req = cache->n_req;
  }

  return obj;
}

static cache_obj_t *Colossus_FIFO_Merge_insert(cache_t *cache,
                                               const request_t *req) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  colossus_segment_t *segment =
      Colossus_FIFO_Merge_check_head_segment(params, obj->obj_size);
  Colossus_FIFO_Merge_attach_obj_head(segment, obj);

  obj->Colossus.last_access_req = cache->n_req;

  return obj;
}

// Object IDs break equal access-time ties consistently with to_evict().
static int Colossus_FIFO_Merge_compare_recency(const void *a, const void *b) {
  const cache_obj_t *left = *(cache_obj_t *const *)a;
  const cache_obj_t *right = *(cache_obj_t *const *)b;
  if (left->Colossus.last_access_req != right->Colossus.last_access_req) {
    return left->Colossus.last_access_req < right->Colossus.last_access_req ? -1
                                                                            : 1;
  }
  return (left->obj_id > right->obj_id) - (left->obj_id < right->obj_id);
}

static cache_obj_t *Colossus_FIFO_Merge_to_evict(cache_t *cache,
                                                 const request_t *req) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;

  colossus_segment_t *victim = Colossus_FIFO_Merge_pick_victim_segment(params);
  if (victim == NULL) return NULL;
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  // least recently used block of the segment the sweep is on
  cache_obj_t *candidate = victim->q_head;
  for (cache_obj_t *check = candidate; check != NULL;
       check = check->queue.next) {
    if (Colossus_FIFO_Merge_compare_recency(&check, &candidate) < 0) {
      candidate = check;
    }
  }

  return candidate;
}

static void Colossus_FIFO_Merge_evict(cache_t *cache, const request_t *req) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;

  colossus_segment_t *victim = Colossus_FIFO_Merge_pick_victim_segment(params);
  if (victim == NULL) return;
  if (victim->n_obj == 0) {  // empty segment --> remove it
    Colossus_FIFO_Merge_unlink_segment(params, victim);
    params->n_segment_evicted++;
    return;
  }

  // sort the segment once --> make dropping (1 - params->retain_ratio) of the
  // segment into a O(n log n) operation instead as we evict the least recently
  // used objects from the segment
  const int64_t n_obj = victim->n_obj;
  cache_obj_t **objects = my_malloc_n(cache_obj_t *, n_obj);
  int64_t i = 0;
  for (cache_obj_t *obj = victim->q_head; obj != NULL; obj = obj->queue.next) {
    objects[i++] = obj;
  }
  qsort(objects, n_obj, sizeof(*objects), Colossus_FIFO_Merge_compare_recency);

  // calculate the budget for the surviving objects (how many bytes will
  // remain, rounded down)
  const int64_t md = cache->obj_md_size;
  int64_t victim_cost = victim->n_byte + md * n_obj;
  const int64_t survivor_budget =
      (int64_t)((double)victim_cost * params->retain_ratio);

  // evict the least recently used objects until the budget is met. At least
  // one obj always goes, so an eviction always makes progress.
  i = 0;
  do {
    cache_obj_t *obj = objects[i++];
    victim_cost -= obj->obj_size + md;
    Colossus_FIFO_Merge_detach_obj(victim, obj);
    cache_evict_base(cache, obj, true);
  } while (i < n_obj && victim_cost > survivor_budget);
  my_free(sizeof(*objects) * n_obj, objects);

  if (victim->n_obj == 0) {  // nothing was retained --> reclaim the segment
    Colossus_FIFO_Merge_unlink_segment(params, victim);
    params->n_segment_evicted++;
    return;
  }

  cache->n_byte_written += victim->n_byte;
  cache->n_obj_written += victim->n_obj;
  params->n_byte_rewritten += victim->n_byte;
  params->n_obj_rewritten += victim->n_obj;
  params->n_segment_merged++;

  // a merged segment is sealed and cannot accept new objects
  if (victim == params->head_segment) {
    Colossus_FIFO_Merge_open_new_segment(params);
  }

  // sweep on toward the head, so the next eviction examines the next segment
  params->next_to_merge = victim->next;
}

static bool Colossus_FIFO_Merge_remove(cache_t *cache, obj_id_t obj_id) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;

  // check whether obj exists in the cache
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) return false;

  colossus_segment_t *segment = (colossus_segment_t *)obj->Colossus.segment;
  DEBUG_ASSERT(segment != NULL);
  Colossus_FIFO_Merge_detach_obj(segment, obj);

  // remove the segment from the cache if it is empty because of the removal
  // if it is the head segment (for writing new segment), keep it instead
  if (segment->n_obj == 0 && segment != params->head_segment) {
    Colossus_FIFO_Merge_unlink_segment(params, segment);
  }

  cache_remove_obj_base(cache, obj, true);
  return true;
}

static void Colossus_FIFO_Merge_print_cache(const cache_t *cache) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)cache->eviction_params;
  printf(
      "Colossus FIFO-Merge: %lld segments of %lld bytes, retain ratio %.4lf\n",
      (long long)params->n_segment, (long long)params->segment_size,
      params->retain_ratio);
  printf(
      "  %lld segments merged, %lld segments reclaimed, %lld objects (%lld "
      "bytes) "
      "rewritten into compacted segments\n",
      (long long)params->n_segment_merged, (long long)params->n_segment_evicted,
      (long long)params->n_obj_rewritten, (long long)params->n_byte_rewritten);
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************

static const char *Colossus_FIFO_Merge_current_params(
    Colossus_FIFO_Merge_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "segment-size-ratio=%.4lf,retain-ratio=%.4lf",
           params->segment_size_ratio, params->retain_ratio);
  return params_str;
}

static void Colossus_FIFO_Merge_parse_params(
    cache_t *cache, const char *cache_specific_params) {
  Colossus_FIFO_Merge_params_t *params =
      (Colossus_FIFO_Merge_params_t *)(cache->eviction_params);
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') params_str++;

    if (key != NULL && strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", Colossus_FIFO_Merge_current_params(params));
      exit(0);
    }

    if (key == NULL || value == NULL) {
      ERROR("invalid parameter string: missing key or value\n");
      exit(1);
    }

    if (strcasecmp(key, "segment-size-ratio") == 0) {
      params->segment_size_ratio = strtod(value, NULL);
      if (!(params->segment_size_ratio > 0 &&
            params->segment_size_ratio <= 1)) {
        ERROR("segment-size-ratio must be in (0, 1], got %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "segment-size") == 0) {
      params->segment_size = strtoll(value, NULL, 0);
      if (params->segment_size <= 0) {
        ERROR("segment-size must be positive, got %s\n", value);
        exit(1);
      }
      // reinsert-ratio is accepted as an alias so that one parameter string
      // can drive both Colossus FIFO variants in a sweep
    } else if (strcasecmp(key, "retain-ratio") == 0 ||
               strcasecmp(key, "reinsert-ratio") == 0) {
      params->retain_ratio = strtod(value, NULL);
      if (!(params->retain_ratio >= 0 && params->retain_ratio < 1)) {
        ERROR("%s must be in [0, 1), got %s\n", key, value);
        exit(1);
      }
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

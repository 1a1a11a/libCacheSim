/** FIFO merge of Colossus segments with a dynamic skip probability and a
 * dynamic merge window.
 *
 * A variant of colossus/FIFO_Merge.c. Instead of having a fixed merge window,
 * this policy introduces skipping and growing merge windows.
 * The skip probability is highest at the tail and decreases toward the head,
 * while the merge window grows from a small value near the tail to a larger
 * value near the head.
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

  colossus_segment_t *pending_cursor;
  int64_t pending_vtime;

  int64_t segment_size;
  double segment_size_ratio;
  double retain_ratio;
  double skip_prob_max;
  int64_t n_merge_max;
  bool anchor_oldest;  // where should the merged segment be anchored (oldest vs
                       // cursor)

  // seed for reproducibility
  bool has_seed;
  uint64_t seed;
  __uint128_t rng_state;

  // statistics: used to report the performance
  int64_t n_byte_rewritten;
  int64_t n_obj_rewritten;
  int64_t n_segment_merged;
  int64_t n_segment_evicted;
  int64_t n_segment_skipped;
} Colossus_FIFO_MergeDSR_params_t;

static const char *Colossus_FIFO_MergeDSR_DEFAULT_CACHE_PARAMS =
    "segment-size-ratio=0.0625,retain-ratio=0.28,skip-prob-max=0.8,n-merge-max="
    "8,"
    "merge-anchor=cursor";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Colossus_FIFO_MergeDSR_free(cache_t *cache);
static bool Colossus_FIFO_MergeDSR_get(cache_t *cache, const request_t *req);
static cache_obj_t *Colossus_FIFO_MergeDSR_find(cache_t *cache,
                                                const request_t *req,
                                                bool update_cache);
static cache_obj_t *Colossus_FIFO_MergeDSR_insert(cache_t *cache,
                                                  const request_t *req);
static cache_obj_t *Colossus_FIFO_MergeDSR_to_evict(cache_t *cache,
                                                    const request_t *req);
static void Colossus_FIFO_MergeDSR_evict(cache_t *cache, const request_t *req);
static bool Colossus_FIFO_MergeDSR_remove(cache_t *cache, obj_id_t obj_id);
static void Colossus_FIFO_MergeDSR_print_cache(const cache_t *cache);
static void Colossus_FIFO_MergeDSR_parse_params(
    cache_t *cache, const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   segment queue helpers                       ****
// ****                                                               ****
// ***********************************************************************

// allocate a new empty segment and push it onto the head of the queue
static colossus_segment_t *Colossus_FIFO_MergeDSR_open_new_segment(
    Colossus_FIFO_MergeDSR_params_t *params) {
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

// drop the remembered skip decision, so the next eviction rolls afresh
static void Colossus_FIFO_MergeDSR_forget_pending_cursor(
    Colossus_FIFO_MergeDSR_params_t *params) {
  params->pending_cursor = NULL;
  params->pending_vtime = -1;
}

// unlink a segment from the queue and free it
static void Colossus_FIFO_MergeDSR_unlink_segment(
    Colossus_FIFO_MergeDSR_params_t *params, colossus_segment_t *segment) {
  // neither the sweep nor a remembered decision may keep naming a segment that
  // is about to be freed
  if (params->next_to_merge == segment) {
    params->next_to_merge = segment->next;
  }
  if (params->pending_cursor == segment) {
    Colossus_FIFO_MergeDSR_forget_pending_cursor(params);
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

static colossus_segment_t *Colossus_FIFO_MergeDSR_pick_cursor_segment(
    Colossus_FIFO_MergeDSR_params_t *params) {
  colossus_segment_t *cursor = params->next_to_merge;
  if (cursor == NULL) cursor = params->tail_segment;
  if (cursor == params->head_segment && cursor != params->tail_segment) {
    cursor = params->tail_segment;
  }
  return cursor;
}

static colossus_segment_t *Colossus_FIFO_MergeDSR_next_candidate(
    Colossus_FIFO_MergeDSR_params_t *params, colossus_segment_t *segment) {
  colossus_segment_t *next = segment->next;
  if (next == NULL || next == params->head_segment) next = params->tail_segment;
  return next;
}

// q: how far the cursor sits along the queue (physically)
// 0 at the tail and 1 at the head
static double Colossus_FIFO_MergeDSR_queue_position(
    Colossus_FIFO_MergeDSR_params_t *params, const colossus_segment_t *cursor) {
  if (params->n_segment <= 1) return 1.0;

  int64_t idx = 0;
  for (const colossus_segment_t *segment = params->tail_segment;
       segment != NULL && segment != cursor; segment = segment->next) {
    idx++;
  }
  return (double)idx / (double)(params->n_segment - 1);
}

// N_merge = 2 + (n_merge_max - 2) * q, so the window widens toward the head.
// n-merge-max=1 pins it to a single segment, which is plain FIFO-Merge.
static int64_t Colossus_FIFO_MergeDSR_window_size(
    Colossus_FIFO_MergeDSR_params_t *params, double q) {
  const double n_merge = 2.0 + ((double)params->n_merge_max - 2.0) * q;
  int64_t n = (int64_t)(n_merge + 0.5);
  if (n < 1) n = 1;
  if (n > params->n_merge_max) n = params->n_merge_max;
  return n;
}

// collect all the segments in the merge window
// it moves from the cursor position to the head, wrapping around to the tail
// NOTE: this skip the newest head segment that is still open for new admissions
static int64_t Colossus_FIFO_MergeDSR_collect_window(
    Colossus_FIFO_MergeDSR_params_t *params, colossus_segment_t *cursor,
    int64_t n_merge, colossus_segment_t **window) {
  int64_t n_window = 0;
  colossus_segment_t *segment = cursor;
  bool wrapped = false;

  while (n_window < n_merge && segment != NULL) {
    if (segment == params->head_segment && segment != cursor) {
      if (wrapped) break;
      wrapped = true;
      segment = params->tail_segment;
      continue;
    }
    if (wrapped && segment == cursor)
      break;  // the whole queue is in the window
    window[n_window++] = segment;
    segment = segment->next;
  }
  return n_window;
}

static __uint128_t Colossus_FIFO_MergeDSR_seed_state(uint64_t seed) {
  uint64_t z = seed, half[2];
  for (int i = 0; i < 2; i++) {
    z += 0x9e3779b97f4a7c15ULL;
    uint64_t x = z;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    half[i] = x ^ (x >> 31);
  }
  return ((__uint128_t)half[1] << 64) | (half[0] | 1ULL);
}

static uint64_t Colossus_FIFO_MergeDSR_next_rand(
    Colossus_FIFO_MergeDSR_params_t *params) {
  if (!params->has_seed) return next_rand();
  params->rng_state *= 0xda942042e4dd58b5;
  return (uint64_t)(params->rng_state >> 64);
}

static colossus_segment_t *Colossus_FIFO_MergeDSR_resolve_cursor(
    Colossus_FIFO_MergeDSR_params_t *params, int64_t vtime) {
  if (params->pending_cursor != NULL && params->pending_vtime == vtime) {
    return params->pending_cursor;
  }

  colossus_segment_t *cursor =
      Colossus_FIFO_MergeDSR_pick_cursor_segment(params);
  if (cursor != NULL && params->skip_prob_max > 0) {
    for (int64_t n_skip = 0; n_skip < params->n_segment; n_skip++) {
      const double q = Colossus_FIFO_MergeDSR_queue_position(params, cursor);
      const double p_skip = params->skip_prob_max * (1.0 - q);
      const double roll =
          (double)(Colossus_FIFO_MergeDSR_next_rand(params) % 1000) / 1000.0;
      if (roll >= p_skip) break;

      cursor = Colossus_FIFO_MergeDSR_next_candidate(params, cursor);
      params->n_segment_skipped++;
    }
  }

  params->pending_cursor = cursor;
  params->pending_vtime = vtime;
  return cursor;
}

// remove obj from the segment holding it
static void Colossus_FIFO_MergeDSR_detach_obj(colossus_segment_t *segment,
                                              cache_obj_t *obj) {
  remove_obj_from_list(&segment->q_head, &segment->q_tail, obj);
  segment->n_obj--;
  segment->n_byte -= obj->obj_size;
}

// insert obj into the segment (at the head)
static void Colossus_FIFO_MergeDSR_attach_obj_head(colossus_segment_t *segment,
                                                   cache_obj_t *obj) {
  prepend_obj_to_head(&segment->q_head, &segment->q_tail, obj);
  segment->n_obj++;
  segment->n_byte += obj->obj_size;
  obj->Colossus.segment = segment;
}

// check whether we need to create a new segment or not to accommodate a new
// object
static colossus_segment_t *Colossus_FIFO_MergeDSR_check_head_segment(
    Colossus_FIFO_MergeDSR_params_t *params, int64_t obj_size) {
  if (params->head_segment == NULL ||
      params->head_segment->n_byte + obj_size > params->segment_size) {
    if (params->head_segment == NULL || params->head_segment->n_byte > 0) {
      return Colossus_FIFO_MergeDSR_open_new_segment(params);
    }
  }
  return params->head_segment;
}

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *Colossus_FIFO_MergeDSR_init(const common_cache_params_t ccache_params,
                                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Colossus-FIFO_MergeDSR", ccache_params,
                                     cache_specific_params);
  cache->cache_init = Colossus_FIFO_MergeDSR_init;
  cache->cache_free = Colossus_FIFO_MergeDSR_free;
  cache->get = Colossus_FIFO_MergeDSR_get;
  cache->find = Colossus_FIFO_MergeDSR_find;
  cache->insert = Colossus_FIFO_MergeDSR_insert;
  cache->evict = Colossus_FIFO_MergeDSR_evict;
  cache->remove = Colossus_FIFO_MergeDSR_remove;
  cache->to_evict = Colossus_FIFO_MergeDSR_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->print_cache = Colossus_FIFO_MergeDSR_print_cache;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 32;  // two queue pointers + colossus metadata
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(Colossus_FIFO_MergeDSR_params_t);
  memset(cache->eviction_params, 0, sizeof(Colossus_FIFO_MergeDSR_params_t));
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;
  params->segment_size = -1;
  Colossus_FIFO_MergeDSR_forget_pending_cursor(params);

  Colossus_FIFO_MergeDSR_parse_params(
      cache, Colossus_FIFO_MergeDSR_DEFAULT_CACHE_PARAMS);

  if (cache_specific_params != NULL) {
    Colossus_FIFO_MergeDSR_parse_params(cache, cache_specific_params);
  }

  // explicit segment-size declared --> use it over the ratio
  if (params->segment_size <= 0) {
    params->segment_size =
        (int64_t)((double)cache->cache_size * params->segment_size_ratio);
  }
  if (params->segment_size <= 0) params->segment_size = 1;
  if (params->segment_size > cache->cache_size) {
    ERROR(
        "Colossus-FIFO_MergeDSR: segment size (%lld) is larger than cache size "
        "(%lld)\n",
        (long long)params->segment_size, (long long)cache->cache_size);
    exit(1);
  }

  int name_n = snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
                        "Colossus-FIFO_MergeDSR-%lld-%.4lf-%.4lf-%lld-%s",
                        (long long)params->segment_size, params->retain_ratio,
                        params->skip_prob_max, (long long)params->n_merge_max,
                        params->anchor_oldest ? "oldest" : "cursor");
  // appended only when seeded, so an unseeded run's name stays byte-identical
  if (params->has_seed && name_n > 0 && name_n < CACHE_NAME_ARRAY_LEN) {
    snprintf(cache->cache_name + name_n, CACHE_NAME_ARRAY_LEN - name_n,
             "-seed=%llu", (unsigned long long)params->seed);
  }

  return cache;
}

static void Colossus_FIFO_MergeDSR_free(cache_t *cache) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;
  colossus_segment_t *segment = params->tail_segment;
  while (segment != NULL) {
    colossus_segment_t *next = segment->next;
    my_free(sizeof(colossus_segment_t), segment);
    segment = next;
  }
  my_free(sizeof(Colossus_FIFO_MergeDSR_params_t), params);
  cache_struct_free(cache);
}

static bool Colossus_FIFO_MergeDSR_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *Colossus_FIFO_MergeDSR_find(cache_t *cache,
                                                const request_t *req,
                                                bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj != NULL && update_cache) {
    // update the object's last access request
    obj->Colossus.last_access_req = cache->n_req;
  }

  return obj;
}

static cache_obj_t *Colossus_FIFO_MergeDSR_insert(cache_t *cache,
                                                  const request_t *req) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  colossus_segment_t *segment =
      Colossus_FIFO_MergeDSR_check_head_segment(params, obj->obj_size);
  Colossus_FIFO_MergeDSR_attach_obj_head(segment, obj);

  obj->Colossus.last_access_req = cache->n_req;

  return obj;
}

// Object IDs break equal access-time ties consistently with to_evict().
static int Colossus_FIFO_MergeDSR_compare_recency(const void *a,
                                                  const void *b) {
  const cache_obj_t *left = *(cache_obj_t *const *)a;
  const cache_obj_t *right = *(cache_obj_t *const *)b;
  if (left->Colossus.last_access_req != right->Colossus.last_access_req) {
    return left->Colossus.last_access_req < right->Colossus.last_access_req ? -1
                                                                            : 1;
  }
  return (left->obj_id > right->obj_id) - (left->obj_id < right->obj_id);
}

static cache_obj_t *Colossus_FIFO_MergeDSR_to_evict(cache_t *cache,
                                                    const request_t *req) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;

  colossus_segment_t *cursor =
      Colossus_FIFO_MergeDSR_resolve_cursor(params, cache->n_req);
  if (cursor == NULL) return NULL;
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  const double q = Colossus_FIFO_MergeDSR_queue_position(params, cursor);
  const int64_t n_merge = Colossus_FIFO_MergeDSR_window_size(params, q);
  colossus_segment_t **window = my_malloc_n(colossus_segment_t *, n_merge);
  const int64_t n_window =
      Colossus_FIFO_MergeDSR_collect_window(params, cursor, n_merge, window);

  cache_obj_t *candidate = NULL;
  for (int64_t w = 0; w < n_window; w++) {
    for (cache_obj_t *check = window[w]->q_head; check != NULL;
         check = check->queue.next) {
      if (candidate == NULL ||
          Colossus_FIFO_MergeDSR_compare_recency(&check, &candidate) < 0) {
        candidate = check;
      }
    }
  }
  my_free(sizeof(*window) * n_merge, window);

  return candidate;
}

static void Colossus_FIFO_MergeDSR_evict(cache_t *cache, const request_t *req) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;

  colossus_segment_t *cursor =
      Colossus_FIFO_MergeDSR_resolve_cursor(params, cache->n_req);
  Colossus_FIFO_MergeDSR_forget_pending_cursor(params);
  if (cursor == NULL) return;

  const double q = Colossus_FIFO_MergeDSR_queue_position(params, cursor);
  const int64_t n_merge = Colossus_FIFO_MergeDSR_window_size(params, q);
  colossus_segment_t **window = my_malloc_n(colossus_segment_t *, n_merge);
  const int64_t n_window =
      Colossus_FIFO_MergeDSR_collect_window(params, cursor, n_merge, window);

  // sort the whole window once --> dropping the coldest blocks is O(n log n)
  // rather than a scan per block evicted, and the sort spans every segment in
  // the window so the blocks that survive are the window's hottest, not each
  // segment's own hottest
  const int64_t md = cache->obj_md_size;
  int64_t n_obj = 0;
  int64_t window_cost = 0;
  for (int64_t w = 0; w < n_window; w++) {
    n_obj += window[w]->n_obj;
    window_cost += window[w]->n_byte + md * window[w]->n_obj;
  }

  if (n_obj == 0) {  // empty window --> reclaim its segments
    for (int64_t w = 0; w < n_window; w++) {
      Colossus_FIFO_MergeDSR_unlink_segment(params, window[w]);
      params->n_segment_evicted++;
    }
    my_free(sizeof(*window) * n_merge, window);
    return;
  }

  cache_obj_t **objects = my_malloc_n(cache_obj_t *, n_obj);
  int64_t i = 0;
  for (int64_t w = 0; w < n_window; w++) {
    for (cache_obj_t *obj = window[w]->q_head; obj != NULL;
         obj = obj->queue.next) {
      objects[i++] = obj;
    }
  }
  qsort(objects, n_obj, sizeof(*objects),
        Colossus_FIFO_MergeDSR_compare_recency);

  // calculate the budget for the surviving objects (how many bytes will
  // remain, rounded down) --> at most a single segment can be kept
  double retain_ratio = params->retain_ratio;
  const double window_cap = 1.0 / (double)n_window;
  if (retain_ratio > window_cap) retain_ratio = window_cap;
  const int64_t survivor_budget = (int64_t)((double)window_cost * retain_ratio);

  // evict the least recently used objects until the budget is met. At least
  // one obj always goes, so an eviction always makes progress.
  i = 0;
  do {
    cache_obj_t *obj = objects[i++];
    window_cost -= obj->obj_size + md;
    Colossus_FIFO_MergeDSR_detach_obj(
        (colossus_segment_t *)obj->Colossus.segment, obj);
    cache_evict_base(cache, obj, true);
  } while (i < n_obj && window_cost > survivor_budget);
  my_free(sizeof(*objects) * n_obj, objects);

  // anchoring the merged segment to the anchor point (oldest or cursor)
  colossus_segment_t *anchor = window[0];
  if (params->anchor_oldest) {
    for (int64_t w = 0; w < n_window; w++) {
      if (window[w] == params->tail_segment) {
        anchor = window[w];
        break;
      }
    }
  }
  for (int64_t w = 0; w < n_window; w++) {
    if (window[w] == anchor) continue;
    while (window[w]->q_head != NULL) {
      cache_obj_t *obj = window[w]->q_head;
      Colossus_FIFO_MergeDSR_detach_obj(window[w], obj);
      Colossus_FIFO_MergeDSR_attach_obj_head(anchor, obj);
    }
    Colossus_FIFO_MergeDSR_unlink_segment(params, window[w]);
    params->n_segment_evicted++;
  }
  my_free(sizeof(*window) * n_merge, window);

  if (anchor->n_obj == 0) {  // nothing was retained --> reclaim the segment too
    Colossus_FIFO_MergeDSR_unlink_segment(params, anchor);
    params->n_segment_evicted++;
    return;
  }

  cache->n_byte_written += anchor->n_byte;
  cache->n_obj_written += anchor->n_obj;
  params->n_byte_rewritten += anchor->n_byte;
  params->n_obj_rewritten += anchor->n_obj;
  params->n_segment_merged++;

  // a merged segment is sealed and cannot accept new objects
  if (anchor == params->head_segment) {
    Colossus_FIFO_MergeDSR_open_new_segment(params);
  }

  // sweep on toward the head, so the next eviction examines the next segment
  params->next_to_merge = anchor->next;
}

static bool Colossus_FIFO_MergeDSR_remove(cache_t *cache, obj_id_t obj_id) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;

  // check whether obj exists in the cache
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) return false;

  colossus_segment_t *segment = (colossus_segment_t *)obj->Colossus.segment;
  DEBUG_ASSERT(segment != NULL);
  Colossus_FIFO_MergeDSR_detach_obj(segment, obj);

  // remove the segment from the cache if it is empty because of the removal
  // if it is the head segment (for writing new segment), keep it instead
  if (segment->n_obj == 0 && segment != params->head_segment) {
    Colossus_FIFO_MergeDSR_unlink_segment(params, segment);
  }

  cache_remove_obj_base(cache, obj, true);
  return true;
}

static void Colossus_FIFO_MergeDSR_print_cache(const cache_t *cache) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)cache->eviction_params;
  printf(
      "Colossus FIFO-MergeDSR: %lld segments of %lld bytes, retain ratio "
      "%.4lf, "
      "max skip prob %.4lf, max merge window %lld\n",
      (long long)params->n_segment, (long long)params->segment_size,
      params->retain_ratio, params->skip_prob_max,
      (long long)params->n_merge_max);
  printf(
      "  %lld merges, %lld segments skipped, %lld segments reclaimed, %lld "
      "objects "
      "(%lld bytes) rewritten into consolidated segments\n",
      (long long)params->n_segment_merged, (long long)params->n_segment_skipped,
      (long long)params->n_segment_evicted, (long long)params->n_obj_rewritten,
      (long long)params->n_byte_rewritten);
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************

static const char *Colossus_FIFO_MergeDSR_current_params(
    Colossus_FIFO_MergeDSR_params_t *params) {
  static __thread char params_str[160];
  int n = snprintf(params_str, sizeof(params_str),
                   "segment-size-ratio=%.4lf,retain-ratio=%.4lf,"
                   "skip-prob-max=%.4lf,n-merge-max=%lld,merge-anchor=%s",
                   params->segment_size_ratio, params->retain_ratio,
                   params->skip_prob_max, (long long)params->n_merge_max,
                   params->anchor_oldest ? "oldest" : "cursor");
  if (params->has_seed && n > 0 && n < (int)sizeof(params_str)) {
    snprintf(params_str + n, sizeof(params_str) - n, ",seed=%llu",
             (unsigned long long)params->seed);
  }
  return params_str;
}

static void Colossus_FIFO_MergeDSR_parse_params(
    cache_t *cache, const char *cache_specific_params) {
  Colossus_FIFO_MergeDSR_params_t *params =
      (Colossus_FIFO_MergeDSR_params_t *)(cache->eviction_params);
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') params_str++;

    if (key != NULL && strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", Colossus_FIFO_MergeDSR_current_params(params));
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
      // can drive all three Colossus FIFO variants in a sweep
    } else if (strcasecmp(key, "retain-ratio") == 0 ||
               strcasecmp(key, "reinsert-ratio") == 0) {
      params->retain_ratio = strtod(value, NULL);
      if (!(params->retain_ratio >= 0 && params->retain_ratio < 1)) {
        ERROR("%s must be in [0, 1), got %s\n", key, value);
        exit(1);
      }
    } else if (strcasecmp(key, "skip-prob-max") == 0) {
      params->skip_prob_max = strtod(value, NULL);
      if (!(params->skip_prob_max >= 0 && params->skip_prob_max <= 1)) {
        ERROR("skip-prob-max must be in [0, 1], got %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "n-merge-max") == 0) {
      params->n_merge_max = strtoll(value, NULL, 0);
      if (params->n_merge_max < 1) {
        ERROR("n-merge-max must be at least 1, got %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "merge-anchor") == 0) {
      if (strcasecmp(value, "cursor") == 0) {
        params->anchor_oldest = false;
      } else if (strcasecmp(value, "oldest") == 0) {
        params->anchor_oldest = true;
      } else {
        ERROR("merge-anchor must be cursor or oldest, got %s\n", value);
        exit(1);
      }
    } else if (strcasecmp(key, "seed") == 0) {
      params->seed = strtoull(value, NULL, 0);
      params->has_seed = true;
      params->rng_state = Colossus_FIFO_MergeDSR_seed_state(params->seed);
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

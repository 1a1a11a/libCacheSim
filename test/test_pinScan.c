// cache_skip_pinned resumes each scan where the last one from the same object
// stopped (cache_skip_pinned_scan in cache/cache.c). These checks drive it on
// bare lists, through the list helpers the eviction algorithms use, and compare
// every answer with a scan from scratch while objects are evicted, moved,
// removed and inserted around the remembered positions.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libCacheSim.h"

#define N_LISTS 3
#define MAX_LIST_LEN 200
#define MAX_ID 4000000

static bool pinned[MAX_ID];

static bool is_pinned(void *ctx, obj_id_t id) {
  (void)ctx;
  return pinned[id];
}

typedef struct {
  cache_obj_t *head;
  cache_obj_t *tail;
  int64_t n;
} list_t;

static list_t lists[N_LISTS];
static obj_id_t next_id;
static uint64_t rng;
static int pin_pct;

static uint64_t rnd(void) {  // xorshift64
  rng ^= rng << 13;
  rng ^= rng >> 7;
  rng ^= rng << 17;
  return rng;
}

static cache_obj_t *scan_from_scratch(cache_obj_t *candidate) {
  cache_obj_t *obj = candidate;
  while (obj != NULL && pinned[obj->obj_id]) obj = obj->queue.prev;
  return obj != NULL ? obj : candidate;
}

static cache_obj_t *new_obj(void) {
  cache_obj_t *obj = create_cache_obj_from_request(NULL);
  obj->obj_id = next_id++;
  pinned[obj->obj_id] = (int)(rnd() % 100) < pin_pct;
  return obj;
}

/** the k-th object from the tail */
static cache_obj_t *nth(const list_t *l, int64_t k) {
  cache_obj_t *obj = l->tail;
  while (k-- > 0) obj = obj->queue.prev;
  return obj;
}

static void unlink_and_free(list_t *l, cache_obj_t *obj) {
  remove_obj_from_list(&l->head, &l->tail, obj);
  free_cache_obj(obj);
  l->n -= 1;
}

/** One random walk of list operations; returns the number of wrong answers. */
static int64_t run(uint64_t seed, int64_t n_steps) {
  rng = seed;
  next_id = 1;
  pin_pct = 80;
  memset(lists, 0, sizeof(lists));
  cache_set_pin_predicate(is_pinned, NULL);

  int64_t n_wrong = 0, n_scans = 0;
  for (int64_t step = 0; step < n_steps; step++) {
    list_t *l = &lists[rnd() % N_LISTS];
    const int op = (int)(rnd() % 100);

    if (l->n == 0 || (op < 25 && l->n < MAX_LIST_LEN)) {
      // insert a new object, mostly at the head as the policies do
      cache_obj_t *obj = new_obj();
      if (rnd() % 10 == 0)
        append_obj_to_tail(&l->head, &l->tail, obj);
      else
        prepend_obj_to_head(&l->head, &l->tail, obj);
      l->n += 1;
    } else if (op < 55) {
      // scan from the tail, or from anywhere (a hand)
      cache_obj_t *from = rnd() % 5 == 0 ? nth(l, rnd() % l->n) : l->tail;
      n_scans += 1;
      if (cache_skip_pinned(from) != scan_from_scratch(from)) n_wrong += 1;
    } else if (op < 75 || l->n >= MAX_LIST_LEN) {
      // evict what the scan returns, a pinned object if the list is all pinned
      cache_obj_t *victim = cache_skip_pinned(l->tail);
      n_scans += 1;
      if (victim != scan_from_scratch(l->tail)) n_wrong += 1;
      unlink_and_free(l, victim);
    } else if (op < 83) {
      move_obj_to_head(&l->head, &l->tail, nth(l, rnd() % l->n));
    } else if (op < 87) {
      move_obj_to_tail(&l->head, &l->tail, nth(l, rnd() % l->n));
    } else if (op < 97) {
      unlink_and_free(l, nth(l, rnd() % l->n));
    } else {
      // change the pin set; the predicate must be installed again after that
      pin_pct = (int)(rnd() % 101);
      for (int i = 0; i < N_LISTS; i++) {
        for (cache_obj_t *obj = lists[i].head; obj != NULL;
             obj = obj->queue.next) {
          if (rnd() % 4 == 0) pinned[obj->obj_id] = !pinned[obj->obj_id];
        }
      }
      cache_set_pin_predicate(is_pinned, NULL);
    }
    if (next_id >= MAX_ID) break;
  }

  for (int i = 0; i < N_LISTS; i++) {
    while (lists[i].tail != NULL) unlink_and_free(&lists[i], lists[i].tail);
  }
  cache_set_pin_predicate(NULL, NULL);
  printf("  seed %llu: %lld scans, %lld wrong\n", (unsigned long long)seed,
         (long long)n_scans, (long long)n_wrong);
  return n_wrong;
}

/** Without a predicate nothing is skipped. */
static bool no_predicate_skips_nothing(void) {
  list_t l = {NULL, NULL, 0};
  next_id = 1;
  pin_pct = 100;
  for (int i = 0; i < 4; i++) {
    prepend_obj_to_head(&l.head, &l.tail, new_obj());
    l.n += 1;
  }
  const bool ok = cache_skip_pinned(l.tail) == l.tail;
  while (l.tail != NULL) unlink_and_free(&l, l.tail);
  return ok;
}

int main(void) {
  bool ok = true;
  printf("no predicate, nothing skipped\n");
  if (!no_predicate_skips_nothing()) {
    printf("  FAIL: an object was skipped with no predicate installed\n");
    ok = false;
  }

  printf("resumed scans agree with scans from scratch\n");
  const uint64_t seeds[] = {1, 2, 3, 42, 20261109};
  for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
    if (run(seeds[i], 500000) != 0) ok = false;
  }

  if (!ok) {
    printf("pin scan check FAILED\n");
    return 1;
  }
  printf("all pin scan checks passed\n");
  return 0;
}

//
// ClockPro replacement algorithm
// https://www.usenix.org/legacy/event/usenix05/tech/general/full_papers/jiang/jiang.pdf
//
// Inspirations are taken from
// https://blog.yufeng.info/wp-content/uploads/2010/08/8-Clock-Pro.pdf
//
// compared with https://bitbucket.org/SamiLehtinen/pyclockpro/src/master/ using
// --ignore-obj-size using cloudPhysicsIO as traces
//
//    Size	      This Implementation	PyClockPro
//   ======	    =======================	==========
//    4897	            0.8363	          0.7420
//    9794	            0.7662	          0.7076
//    14692	            0.6435	          0.6214
//    19589	            0.5670	          0.5848
//    24487	            0.5092	          0.5654
//    29384	            0.4955	          0.5653
//    34281	            0.4726	          0.5646
//    39179	            0.4574	          0.5049
//    44076	            0.4384	          0.4302
//    48974	            0.4301	          0.4301
//
// one thing to note is the difference in the clock hand movement (this
// implementation vs PyClockPro) this implementation checks the object pointed
// by the hand first before moving the hand (as per the material in
// blog.yufeng.info) PyClockPro implementation moves the hand first before
// checking the object pointed by the hand
//
// libCacheSim
//
// Created by Marthen on 2/12/25.
// Copyright © 2025 Marthen. All rights reserved.
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define USE_BELADY
#undef USE_BELADY

typedef struct ClockPro_params {
  cache_obj_t *hand_hot;
  cache_obj_t *hand_cold;
  cache_obj_t *hand_test;

  int64_t mem_cold_max;
  int64_t mem_cold;
  int64_t mem_test;
  int64_t mem_hot;

  hashtable_t *ht_test;

  bool init_ref;
} ClockPro_params_t;

static const char *DEFAULT_PARAMS = "init-ref=0,init-ratio-cold=1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void ClockPro_parse_params(cache_t *cache,
                                  const char *cache_specific_params);
static void ClockPro_free(cache_t *cache);
static bool ClockPro_get(cache_t *cache, const request_t *req);
static cache_obj_t *ClockPro_find(cache_t *cache, const request_t *req,
                                  bool update_cache);
static cache_obj_t *ClockPro_insert(cache_t *cache, const request_t *req);
static void ClockPro_evict(cache_t *cache, const request_t *req);
static bool ClockPro_remove(cache_t *cache, obj_id_t obj_id);
static bool ClockPro_can_insert(cache_t *cache, const request_t *req);
static void ClockPro_promote(cache_t *cache, cache_obj_t *obj);
static void ClockPro_run_test(cache_t *cache);
static void ClockPro_run_cold(cache_t *cache);
static void ClockPro_run_hot(cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief initialize a ClockPro cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params Clock specific parameters as a string
 */
cache_t *ClockPro_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("ClockPro", ccache_params, cache_specific_params);
  cache->cache_init = ClockPro_init;
  cache->cache_free = ClockPro_free;
  cache->get = ClockPro_get;
  cache->find = ClockPro_find;
  cache->insert = ClockPro_insert;
  cache->evict = ClockPro_evict;
  cache->remove = ClockPro_remove;
  cache->can_insert = ClockPro_can_insert;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->obj_md_size = 0;

  cache->eviction_params = my_malloc_n(ClockPro_params_t, 1);
  ClockPro_params_t *params = (ClockPro_params_t *)(cache->eviction_params);

  params->hand_hot = NULL;
  params->hand_cold = NULL;
  params->hand_test = NULL;
  params->mem_cold = 0;
  params->mem_test = 0;
  params->mem_hot = 0;
  params->mem_cold_max =
      cache->cache_size;  // default to the cache size (fallback)
  /* ht_test tracks objects in their test period. It is private to ClockPro,
   * so cache_struct_init never sizes it, and hard-coding HASH_POWER_DEFAULT
   * meant cachesim's --hashpower reached only the main table: at a 1GB cache
   * size, --hashpower=12 still left the run at 77.7MiB peak RSS against lru's
   * 14.0MiB, the difference being this table's 2^23 pointer slots.
   *
   * Cap rather than mirror. Mirroring the main table would raise the default
   * from 205.4MiB to 269.4MiB here, because cachesim's default hashpower is 24
   * while HASH_POWER_DEFAULT is 23 -- a request to use less memory must not
   * turn into more for everyone who makes no request. The main table has
   * already applied the "0 means HASH_POWER_DEFAULT" sentinel and the upper
   * bound, so reading it back is enough. ClockPro's miss ratio does not depend
   * on this table's size -- measured identical at hashpower 12, 18 and 24 --
   * so a smaller one costs rehashing and nothing else. */
  int test_hashpower = HASH_POWER_DEFAULT;
  if (cache->hashtable->hashpower < test_hashpower) {
    test_hashpower = cache->hashtable->hashpower;
  }
  params->ht_test = create_hashtable(test_hashpower);

  ClockPro_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    ClockPro_parse_params(cache, cache_specific_params);
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void ClockPro_free(cache_t *cache) {
  ClockPro_params_t *params = (ClockPro_params_t *)(cache->eviction_params);
  free_hashtable(params->ht_test);
  my_free(sizeof(ClockPro_params_t), params);
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
 * @return
 */
static bool ClockPro_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief check whether an object is in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 * if true, the object is promoted or set as referenced
 * and if the object is expired, it is removed from the cache
 * @return true on hit, false on miss
 */
static cache_obj_t *ClockPro_find(cache_t *cache, const request_t *req,
                                  bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj != NULL && update_cache) {
    if (!obj->clockpro.referenced) {
      obj->clockpro.referenced = true;
    }
  }

  return obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *ClockPro_insert(cache_t *cache, const request_t *req) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;

  // request to insert a test object
  cache_obj_t *test_obj = hashtable_find_obj_id(params->ht_test, req->obj_id);
  if (test_obj != NULL) {
    // A page in the test period is being re-referenced, so it becomes hot.
    // Promoting the test entry in place is not enough: it lives in ht_test, not
    // in cache->hashtable, and cache_insert_base() was never called for it. The
    // page therefore counted as hot in mem_hot while staying invisible to
    // cache->find() and absent from occupied_byte/n_obj -- and when the cold
    // hand later evicted it, cache_evict_base() tripped the
    // "delete an object that is not in the table" assertion in the hash table.
    // Hand its clock slot over to a real resident object instead.
    cache_obj_t *next = test_obj->queue.next;
    cache_obj_t *prev = test_obj->queue.prev;
    cache_obj_t *obj = cache_insert_base(cache, req);
    obj->clockpro.referenced = test_obj->clockpro.referenced;
    obj->clockpro.status = CLOCKPRO_TEST;  // so promote() debits mem_test
    if (next == test_obj) {  // the test entry was alone on the clock
      obj->queue.next = obj;
      obj->queue.prev = obj;
    } else {
      obj->queue.next = next;
      obj->queue.prev = prev;
      next->queue.prev = obj;
      prev->queue.next = obj;
    }
    if (params->hand_hot == test_obj) params->hand_hot = obj;
    if (params->hand_cold == test_obj) params->hand_cold = obj;
    if (params->hand_test == test_obj) params->hand_test = obj;
    hashtable_delete(params->ht_test, test_obj);

    ClockPro_promote(cache, obj);
    return obj;
  }

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->clockpro.referenced = params->init_ref;
  obj->clockpro.status = CLOCKPRO_COLD;

  if (params->hand_hot == NULL) {  // Initial insertion
    prepend_obj_to_head(&params->hand_hot, &params->hand_hot, obj);
    params->hand_hot->queue.next = params->hand_hot;
    params->hand_hot->queue.prev = params->hand_hot;
    params->hand_cold = params->hand_hot;
    params->hand_test = params->hand_hot;
  } else {
    cache_obj_t *hand_hot_prev = params->hand_hot->queue.prev;
    prepend_obj_to_head(&params->hand_hot, &hand_hot_prev, obj);
    obj->queue.prev = hand_hot_prev;
    obj->queue.prev->queue.next = obj;
    params->hand_hot = obj->queue.next;
  }

  params->mem_cold += obj->obj_size;

  return obj;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void ClockPro_evict(cache_t *cache, const request_t *req) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;

  if (params->hand_cold == NULL) {
    return;  // nothing has ever been inserted; there is no clock to turn
  }

  // One ClockPro_run_cold() is one tick of the cold hand, and a tick frees a
  // page only when it lands on an unreferenced cold one -- otherwise it just
  // advances the hand (or promotes). cache_get_base() hides that behind its own
  // `while (occupied + size > cache_size) evict()` retry loop, but a caller
  // that frees space up front and expects evict() to free a page sees a no-op
  // and gives up. Tick until a page actually goes, which is exactly the
  // sequence cache_get_base() would have driven, so its result is unchanged.
  // (A run that terminated before never needed the lap guards below -- for it,
  // cache_get_base() would have spun on evict() forever.)
  const int64_t occupied_before = cache->occupied_byte;
  while (cache->occupied_byte == occupied_before) {
    // A lap of the cold hand. None of its non-freeing paths removes an object,
    // so `start` stays valid and the hand advances one page per tick; coming
    // back round to `start` means no page on the clock is evictable right now.
    const cache_obj_t *start = params->hand_cold;
    bool lapped = false;
    while (cache->occupied_byte == occupied_before && !lapped) {
      ClockPro_run_cold(cache);
      lapped = (params->hand_cold == start);
    }
    if (cache->occupied_byte != occupied_before) break;

    // The lap found nothing: the resident set is all hot, so the cold hand has
    // nothing it is allowed to evict. Turn the hot hand to demote one page to
    // cold and try again -- ClockPro's own way of refilling the cold list.
    // cache_get_base() cannot reach this: it only evicts while the cache is
    // full, and promote() caps mem_hot at cache_size - mem_cold_max, so a full
    // cache always leaves mem_cold >= mem_cold_max pages on the cold hand.
    if (params->mem_hot <= 0) break;
    const cache_obj_t *hot_start = params->hand_hot;
    const int64_t hot_before = params->mem_hot;
    bool hot_lapped = false;
    while (params->mem_hot == hot_before &&
           cache->occupied_byte == occupied_before && !hot_lapped) {
      ClockPro_run_hot(cache);
      hot_lapped = (params->hand_hot == hot_start);
    }
    if (params->mem_hot == hot_before &&
        cache->occupied_byte == occupied_before) {
      break;  // nothing could be demoted either; give up rather than spin
    }
  }
}

/**
 * @brief remove the given object from the cache
 * note that eviction should not call this function, but rather call
 * `cache_evict_base` because we track extra metadata during eviction
 *
 * and this function is different from eviction
 * because this is used for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj
 */
static void ClockPro_remove_obj(cache_t *cache, cache_obj_t *obj) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  cache_obj_t *hand_hot_prev = params->hand_hot->queue.prev;

  if (obj->clockpro.status == CLOCKPRO_TEST) {
    params->mem_test -= obj->obj_size;
  } else if (obj->clockpro.status == CLOCKPRO_COLD) {
    params->mem_cold -= obj->obj_size;
  } else if (obj->clockpro.status == CLOCKPRO_HOT) {
    params->mem_hot -= obj->obj_size;
  }

  if (params->hand_test == obj) {
    params->hand_test = obj->queue.next;
  }
  if (params->hand_cold == obj) {
    params->hand_cold = obj->queue.next;
  }
  if (params->hand_hot == obj) {
    params->hand_hot = obj->queue.next;
  }

  remove_obj_from_list(&params->hand_hot, &hand_hot_prev, obj);
  cache_remove_obj_base(cache, obj, true);
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
static bool ClockPro_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  ClockPro_remove_obj(cache, obj);

  return true;
}

static bool ClockPro_can_insert(cache_t *cache, const request_t *req) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;
  return cache_can_insert_default(cache, req) &&
         (params->mem_cold + req->obj_size <= params->mem_cold_max);
}

static void ClockPro_run_test(cache_t *cache) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->hand_test;

  if (obj->clockpro.status != CLOCKPRO_TEST) {
    params->hand_test = obj->queue.next;
    return;
  }

  params->mem_test -= obj->obj_size;

  if (params->mem_cold_max > obj->obj_size) {
    params->mem_cold_max -= obj->obj_size;
  } else {
    params->mem_cold_max = 0;
  }

  if (params->hand_hot == obj) {
    params->hand_hot = obj->queue.next;
  }
  if (params->hand_cold == obj) {
    params->hand_cold = obj->queue.next;
  }

  cache_obj_t *hand_test_prev = params->hand_test->queue.prev;
  remove_obj_from_list(&params->hand_test, &hand_test_prev, obj);
  hashtable_delete(params->ht_test, obj);

  while (params->mem_cold > params->mem_cold_max) {
    ClockPro_run_cold(cache);
  }
}

static void ClockPro_run_cold(cache_t *cache) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->hand_cold;

  if (obj->clockpro.status != CLOCKPRO_COLD) {
    params->hand_cold = obj->queue.next;
    return;
  }

  if (obj->clockpro.referenced) {
    ClockPro_promote(cache, obj);
    return;
  }

  params->mem_cold -= obj->obj_size;

  while (params->mem_test + obj->obj_size > cache->cache_size) {
    ClockPro_run_test(cache);
  }

  request_t req;
  copy_cache_obj_to_request(&req, obj);
  cache_obj_t *demoted_obj = hashtable_insert(params->ht_test, &req);
  demoted_obj->clockpro.referenced = params->init_ref;
  demoted_obj->clockpro.status = CLOCKPRO_TEST;

  params->mem_test += obj->obj_size;

  demoted_obj->queue.next = params->hand_cold->queue.next;
  demoted_obj->queue.prev = params->hand_cold->queue.prev;

  params->hand_cold->queue.next->queue.prev = demoted_obj;
  params->hand_cold->queue.prev->queue.next = demoted_obj;

  if (params->hand_hot == obj) {
    params->hand_hot = demoted_obj;
  }
  if (params->hand_test == obj) {
    params->hand_test = demoted_obj;
  }

  cache_evict_base(cache, obj, true);
  params->hand_cold = demoted_obj->queue.next;
}

static void ClockPro_run_hot(cache_t *cache) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->hand_hot;

  if (obj->clockpro.status != CLOCKPRO_HOT) {
    params->hand_hot = obj->queue.next;
    return;
  }

  if (obj->clockpro.referenced) {
    obj->clockpro.referenced = false;
    params->hand_hot = obj->queue.next;
    return;
  }

  while (params->mem_cold + obj->obj_size > params->mem_cold_max) {
    ClockPro_run_cold(cache);
  }

  obj->clockpro.status = CLOCKPRO_COLD;
  obj->clockpro.referenced = params->init_ref;

  if (params->hand_cold == obj) {
    params->hand_cold = obj->queue.next;
  }
  if (params->hand_test == obj) {
    params->hand_test = obj->queue.next;
  }

  cache_obj_t *hand_hot_next = params->hand_hot->queue.next;
  move_obj_to_tail(&hand_hot_next, &params->hand_hot, obj);
  params->hand_hot = obj->queue.next;

  params->mem_hot -= obj->obj_size;
  params->mem_cold += obj->obj_size;
}

static void ClockPro_promote(cache_t *cache, cache_obj_t *obj) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;

  if (obj->clockpro.status == CLOCKPRO_TEST) {
    if (params->mem_cold_max + (int64_t)obj->obj_size > cache->cache_size) {
      params->mem_cold_max = cache->cache_size;
    } else {
      params->mem_cold_max += (int64_t)obj->obj_size;
    }
  }

  // Step the cold and test hands off `obj` before making room, not after.
  // ClockPro_run_hot() below can re-enter ClockPro_run_cold(), which reads
  // hand_cold; while the hand still pointed at `obj` -- a referenced cold page
  // -- that re-entry called ClockPro_promote(obj) again, and the same three
  // frames recursed until the stack ran out. Moving the two hands first makes
  // the re-entry look at the next page instead. Any run that terminated before
  // is unaffected: it either never re-entered run_cold() from here, or it did
  // and then overflowed the stack.
  if (params->hand_cold == obj) {
    params->hand_cold = obj->queue.next;
  }
  if (params->hand_test == obj) {
    params->hand_test = obj->queue.next;
  }

  // Make room in the hot list. `mem_hot > 0` is a termination guard, not a
  // policy change: when nothing is hot, every ClockPro_run_hot() call takes the
  // "hand_hot is not pointing at a hot object" branch, which only advances the
  // hand and cannot lower mem_hot any further -- so once mem_hot reaches 0 with
  // the size condition still unsatisfied, the loop can never exit on its own.
  // That state is reachable whenever the hot budget (cache_size - mem_cold_max)
  // is smaller than obj_size, in particular right after startup, where
  // mem_cold_max == cache_size makes the budget 0 and the condition
  // unsatisfiable for any object. Any execution that terminated before still
  // takes exactly the same path, because it never reached mem_hot == 0 here.
  while ((params->mem_hot + obj->obj_size) >
             (cache->cache_size - params->mem_cold_max) &&
         params->mem_hot > 0) {
    ClockPro_run_hot(cache);
  }

  clockpro_status_e old_status = obj->clockpro.status;
  obj->clockpro.status = CLOCKPRO_HOT;
  obj->clockpro.referenced = params->init_ref;
  cache_obj_t *hand_hot_next = params->hand_hot->queue.next;
  move_obj_to_tail(&hand_hot_next, &params->hand_hot, obj);
  obj->queue.next = hand_hot_next;
  hand_hot_next->queue.prev = obj;

  params->hand_hot = obj->queue.next;

  if (old_status == CLOCKPRO_COLD) {
    params->mem_cold -= obj->obj_size;
  } else if (old_status == CLOCKPRO_TEST) {
    params->mem_test -= obj->obj_size;
  }

  params->mem_hot += obj->obj_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *ClockPro_current_params(cache_t *cache,
                                           ClockPro_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "init-ref=%d\n", params->init_ref);
  return params_str;
}

static void ClockPro_parse_params(cache_t *cache,
                                  const char *cache_specific_params) {
  ClockPro_params_t *params = (ClockPro_params_t *)(cache->eviction_params);
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "init-ref") == 0) {
      params->init_ref = strtol(value, &end, 10);
    } else if (strcasecmp(key, "init-ratio-cold") == 0) {
      const double ratio = strtod(value, &end);
      params->mem_cold_max = (int64_t)(cache->cache_size * ratio);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n",
             ClockPro_current_params(cache, params));
      free(old_params_str);
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
    }
  }
  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

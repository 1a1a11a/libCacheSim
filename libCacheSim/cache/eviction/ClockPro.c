//
// ClockPro replacement algorithm
// https://www.usenix.org/legacy/event/usenix05/tech/general/full_papers/jiang/jiang.pdf
//
// Inspirations are taken from
// https://blog.yufeng.info/wp-content/uploads/2010/08/8-Clock-Pro.pdf
//
// compared with https://bitbucket.org/SamiLehtinen/pyclockpro/src/master/ using --ignore-obj-size
// using cloudPhysicsIO as traces
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
// one thing to note is the difference in the clock hand movement (this implementation vs PyClockPro)
// this implementation checks the object pointed by the hand first before moving the hand (as per the material in blog.yufeng.info)
// PyClockPro implementation moves the hand first before checking the object pointed by the hand
//
// libCacheSim
//
// Created by Marthen on 2/12/25.
// Copyright © 2025 Marthen. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define USE_BELADY
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

static void ClockPro_parse_params(cache_t *cache, const char *cache_specific_params);
static void ClockPro_free(cache_t *cache);
static bool ClockPro_get(cache_t *cache, const request_t *req);
static cache_obj_t *ClockPro_find(cache_t *cache, const request_t *req, bool update_cache);
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
cache_t *ClockPro_init(const common_cache_params_t ccache_params, const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("ClockPro", ccache_params, cache_specific_params);
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
  params->mem_cold_max = cache->cache_size; // default to the cache size (fallback)
  params->ht_test = create_hashtable(HASH_POWER_DEFAULT);

  ClockPro_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    ClockPro_parse_params(cache, cache_specific_params);
  }

  return cache;
};

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
static cache_obj_t *ClockPro_find(cache_t *cache, const request_t *req, const bool update_cache) {
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
    ClockPro_promote(cache, test_obj);
    return test_obj;
  }

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->clockpro.referenced = params->init_ref;
  obj->clockpro.status = CLOCKPRO_COLD;

  if (params->hand_hot == NULL) { // Initial insertion
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
  ClockPro_run_cold(cache);
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
static bool ClockPro_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  ClockPro_remove_obj(cache, obj);

  return true;
}

static bool ClockPro_can_insert(cache_t *cache, const request_t *req) {
  ClockPro_params_t *params = (ClockPro_params_t *)cache->eviction_params;
  return cache_can_insert_default(cache, req) && (params->mem_cold + req->obj_size <= params->mem_cold_max);
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

  while ((params->mem_hot + obj->obj_size) > (cache->cache_size - params->mem_cold_max)) {
    ClockPro_run_hot(cache);
  }

  if (params->hand_cold == obj) {
    params->hand_cold = obj->queue.next;
  }
  if (params->hand_test == obj) {
    params->hand_test = obj->queue.next;
  }

  clockpro_status_e old_status = obj->clockpro.status;
  obj->clockpro.status = CLOCKPRO_HOT;
  obj->clockpro.referenced = params->init_ref;
  cache_obj_t *hand_hot_next = params->hand_hot->queue.next;
  move_obj_to_tail(&hand_hot_next, &params->hand_hot, obj);
  obj->queue.next  = hand_hot_next;
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
static const char *ClockPro_current_params(cache_t *cache, ClockPro_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "init-ref=%d\n", params->init_ref);
  return params_str;
}

static void ClockPro_parse_params(cache_t *cache, const char *cache_specific_params) {
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
      params->mem_cold_max = (int64_t)((double)cache->cache_size * ratio);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", ClockPro_current_params(cache, params));
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

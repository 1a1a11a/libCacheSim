//
//  a novel approximate LFU structure (TinyLFU) admission control
//  any eviction policy
//  combined with a windowed LRU structure (W-TinyLFU)
//  to resist burst traffic of temporal hot objects
//
//  hill-climb=1 adapts the window size with the hill climber in Adaptive
//  Software Cache Management (Middleware'18): every 10 x cache size bytes of
//  requests, move the window by 5% (1% to 80%), and turn around if the hit
//  ratio dropped. Different from the paper, the window does not go to 0%
//  (every request would miss), a tie does not turn around (as in Caffeine),
//  and the period is in bytes (the same for unit-size objects)
//
//  NOTE: it is NOT thread-safe
//
//  part of the details borrowed from https://github.com/mandreyel/w-tinylfu
//
//  WTinyLFU.c
//  libCacheSim
//
//  Created by Ziyue on 14/1/2023.
//

#include <errno.h>
#include <math.h>

#include "dataStructure/hashtable/hashtable.h"
#include "dataStructure/minimalIncrementCBF.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MODE
#undef DEBUG_MODE
#define DEBUG_MODE_2
#undef DEBUG_MODE_2

typedef struct WTinyLFU_params {
  cache_t *LRU;         // LRU as windowed LRU
  cache_t *main_cache;  // any eviction policy
  double window_size;
  int64_t n_admit_bytes;
  struct minimalIncrementCBF *CBF;
  size_t max_request_num;
  size_t request_counter;
  char main_cache_type[32];

  request_t *req_local;

  bool hill_climb;
  int window_step;
  int climb_direction;
  int64_t n_climb_req;
  int64_t n_climb_hit;
  int64_t n_climb_byte;
  double prev_hit_ratio;
} WTinyLFU_params_t;

static const char *DEFAULT_PARAMS = "main-cache=SLRU,window-size=0.01";

#define MAX_WINDOW_STEP 16
static double WTinyLFU_window_ratio(int step) {
  return step == 0 ? 0.01 : 0.05 * step;
}

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void WTinyLFU_free(cache_t *cache);
static cache_obj_t *WTinyLFU_find(cache_t *cache, const request_t *req,
                                  const bool update);
static bool WTinyLFU_get(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFU_to_evict(cache_t *cache, const request_t *req);
static void WTinyLFU_evict(cache_t *cache, const request_t *req);
static bool WTinyLFU_remove(cache_t *cache, obj_id_t obj_id);
static void WTinyLFU_parse_params(cache_t *cache,
                                  const char *cache_specific_params);

bool WTinyLFU_can_insert(cache_t *cache, const request_t *req);
static int64_t WTinyLFU_get_occupied_byte(const cache_t *cache);
static int64_t WTinyLFU_get_n_obj(const cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *WTinyLFU_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("WTinyLFU", ccache_params, cache_specific_params);
  cache->cache_init = WTinyLFU_init;
  cache->cache_free = WTinyLFU_free;
  cache->get = WTinyLFU_get;
  cache->find = WTinyLFU_find;
  cache->insert = WTinyLFU_insert;
  cache->evict = WTinyLFU_evict;
  cache->remove = WTinyLFU_remove;
  cache->to_evict = WTinyLFU_to_evict;
  cache->can_insert = WTinyLFU_can_insert;
  cache->get_occupied_byte = WTinyLFU_get_occupied_byte;
  cache->get_n_obj = WTinyLFU_get_n_obj;

  cache->eviction_params =
      (WTinyLFU_params_t *)malloc(sizeof(WTinyLFU_params_t));
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(WTinyLFU_params_t));

  /* obj_md_size is set once main_cache exists; it is read from main_cache,
   * which is only built further down */
  cache->obj_md_size = 0;

  WTinyLFU_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    WTinyLFU_parse_params(cache, cache_specific_params);
  }

  if (params->hill_climb) {
    if (strcasecmp(params->main_cache_type, "LRU") != 0 &&
        strcasecmp(params->main_cache_type, "SLRU") != 0) {
      ERROR("WTinyLFU hill-climb only supports LRU and SLRU main caches\n");
    }
    params->window_step =
        (int)MIN(lround(params->window_size / 0.05), MAX_WINDOW_STEP);
    params->window_size = WTinyLFU_window_ratio(params->window_step);
    params->climb_direction = 1;
  }

  common_cache_params_t ccache_params_local = ccache_params;

  ccache_params_local.cache_size *= params->window_size;
  params->LRU = LRU_init(ccache_params_local, NULL);
  ccache_params_local.cache_size = ccache_params.cache_size;
  ccache_params_local.cache_size -= params->LRU->cache_size;

  if (strcasecmp(params->main_cache_type, "LRU") == 0) {
    params->main_cache = LRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "SLRU") == 0) {
    params->main_cache = SLRU_init(ccache_params_local, "seg-size=1:4");
  } else if (strcasecmp(params->main_cache_type, "LFU") == 0) {
    params->main_cache = LFU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "FIFO") == 0) {
    params->main_cache = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "FIFO-Reinsertion") == 0) {
    params->main_cache = FIFO_Reinsertion_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "ARC") == 0) {
    params->main_cache = ARC_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LeCaR") == 0) {
    params->main_cache = LeCaR_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Cacheus") == 0) {
    params->main_cache = Cacheus_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Hyperbolic") == 0) {
    params->main_cache = Hyperbolic_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LHD") == 0) {
    params->main_cache = LHD_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "SIEVE") == 0) {
    params->main_cache = Sieve_init(ccache_params_local, NULL);
  } else {
    ERROR("WTinyLFU does not support %s \n", params->main_cache_type);
  }

  if (ccache_params.consider_obj_metadata) {
    /* The window and the main cache can charge different per-object overheads
     * (LRU and SLRU reserve 16 bytes, FIFO none), so neither value alone
     * describes the pair. This one is what the parent-level size check in
     * cache_can_insert_default() uses, so take the larger of the two: an object
     * that does not fit under the heavier policy does not fit in this cache.
     * WTinyLFU_can_insert() checks each sub-cache against its own overhead. */
    /* Every incoming object is inserted into the window, and this field is
     * what cache_get_base()'s capacity loop charges an incoming object, so it
     * is the window's overhead rather than the pair's maximum. The other two
     * sites each charge the cache the object is actually entering:
     * WTinyLFU_can_insert() the window, WTinyLFU_evict() the main cache on
     * promotion. Using the maximum here made the loop reserve up to 40 bytes
     * for a 16-byte window insertion with an ARC, LeCaR or Cacheus main
     * cache. */
    cache->obj_md_size = params->LRU->obj_md_size;
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "WTinyLFU-w%.2lf-%s%s",
           params->window_size, params->main_cache_type,
           params->hill_climb ? "-climb" : "");

  params->req_local = new_request();
  params->n_admit_bytes = 0;

  params->max_request_num =
      32 * params->main_cache->cache_size;  // sample size is 32

  params->CBF =
      (struct minimalIncrementCBF *)malloc(sizeof(struct minimalIncrementCBF));
  DEBUG_ASSERT(params->CBF != NULL);
  params->CBF->ready = 0;

  // TODO @ Ziyue: how to set entries and error rate?
  int ret = minimalIncrementCBF_init(params->CBF,
                                     params->main_cache->cache_size, 0.001);
  if (ret != 0) {
    ERROR("CBF init failed\n");
  }

#ifdef DEBUG_MODE
  minimalIncrementCBF_print(params->CBF);
#endif

  params->request_counter = 0;  // initialize request counter

#if defined(TRACK_DEMOTION)
  params->LRU->track_demotion = false;
  params->main_cache->track_demotion = false;
#endif

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void WTinyLFU_free(cache_t *cache) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->main_cache->cache_free(params->main_cache);

  minimalIncrementCBF_free(params->CBF);
  free(params->CBF);
  free_request(params->req_local);
  free(params);

  cache_struct_free(cache);
}

static void WTinyLFU_resize_window(cache_t *cache, const request_t *req) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);
  cache_t *main_cache = params->main_cache;
  double window_ratio = WTinyLFU_window_ratio(params->window_step);
  params->LRU->cache_size = (int64_t)(cache->cache_size * window_ratio);
  int64_t main_size = cache->cache_size - params->LRU->cache_size;
  if (strcasecmp(params->main_cache_type, "SLRU") == 0) {
    SLRU_resize(main_cache, main_size);
  } else {
    main_cache->cache_size = main_size;
  }
  while (main_cache->get_occupied_byte(main_cache) > main_cache->cache_size) {
    main_cache->evict(main_cache, req);
  }
}

static void WTinyLFU_climb(cache_t *cache, const request_t *req, bool hit) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);
  params->n_climb_req += 1;
  params->n_climb_hit += hit;
  params->n_climb_byte += req->obj_size;
  if (params->n_climb_byte < 10 * cache->cache_size) {
    return;
  }

  double hit_ratio = (double)params->n_climb_hit / params->n_climb_req;
  if (hit_ratio < params->prev_hit_ratio) {
    params->climb_direction = -params->climb_direction;
  }
  params->prev_hit_ratio = hit_ratio;
  params->n_climb_req = params->n_climb_hit = params->n_climb_byte = 0;

  params->window_step += params->climb_direction;
  params->window_step = MAX(0, MIN(MAX_WINDOW_STEP, params->window_step));
  WTinyLFU_resize_window(cache, req);
}

static bool WTinyLFU_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_byte == 0);
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);

  bool ck = cache_get_base(cache, req);
  if (params->hill_climb) {
    WTinyLFU_climb(cache, req, ck);
  }
  return ck;
}

/* Record one access in the frequency sketch, and halve every counter after
 * max_request_num accesses. As in the W-TinyLFU paper and Caffeine, every
 * access counts once: a hit in either the window or the main cache, or the
 * insertion that follows a miss. */
static void WTinyLFU_record_access(WTinyLFU_params_t *params, obj_id_t obj_id) {
  minimalIncrementCBF_add(params->CBF, (void *)&obj_id, sizeof(obj_id_t));

  params->request_counter++;
  if (params->request_counter >= params->max_request_num) {
    params->request_counter = 0;
    minimalIncrementCBF_decay(params->CBF);
  }
}

static cache_obj_t *WTinyLFU_find(cache_t *cache, const request_t *req,
                                  bool update_cache) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);

  cache_obj_t *obj_window = NULL;
  cache_obj_t *obj_main = NULL;

  obj_window = params->LRU->find(params->LRU, req, update_cache);
  obj_main = params->main_cache->find(params->main_cache, req, update_cache);

  cache_obj_t *obj = obj_window != NULL ? obj_window : obj_main;

  if (!update_cache) {
    return obj;
  }

  // a hit counts whether it lands in the window or in the main cache; a miss
  // is counted when WTinyLFU_insert admits the object to the window
  if (obj != NULL) {
    WTinyLFU_record_access(params, req->obj_id);
  }

  return obj;
}

cache_obj_t *WTinyLFU_insert(cache_t *cache, const request_t *req) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;
  obj = params->LRU->insert(params->LRU, req);

  WTinyLFU_record_access(params, req->obj_id);

#if defined(TRACK_DEMOTION)
  obj->create_time = cache->n_req;
#endif

  return obj;
}

static cache_obj_t *WTinyLFU_to_evict(cache_t *cache, const request_t *req) {
  // Warning: don't use this function
  DEBUG_ASSERT(false);
  return NULL;
}

static void WTinyLFU_evict(cache_t *cache, const request_t *req) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);

  cache_t *window = params->LRU;
  cache_t *main_cache = params->main_cache;

  bool evicted = false;
  while (!evicted) {
    if (window->get_occupied_byte(window) > 0) {
      cache_obj_t *window_victim = window->to_evict(window, req);
      DEBUG_ASSERT(window_victim != NULL);

      // window victim req is different from req
      copy_cache_obj_to_request(params->req_local, window_victim);

      /** only when main_cache is full, evict an obj from the main_cache **/

      // if main_cache has enough space, insert the obj into main_cache
      /* charge the main cache its own per-object overhead, not the composite's.
       * cache->obj_md_size is the larger of the two sub-caches, so that a
       * caller asking the composite what it reserves is not told less than it
       * really does; using it here would bill a FIFO or Clock main cache for
       * the window's 16 bytes and call it full early. */
      if (main_cache->get_occupied_byte(main_cache) +
              params->req_local->obj_size + main_cache->obj_md_size <=
          main_cache->cache_size) {
        main_cache->insert(main_cache, params->req_local);

#if defined(TRACK_DEMOTION)
        printf("%ld keep %ld %ld\n", cache->n_req, window_victim->create_time,
               window_victim->next_access_vtime);
#endif
        params->n_admit_bytes += params->req_local->obj_size;

        window->remove(window, window_victim->obj_id);

      } else {
        // compare the frequency of window_victim and main_cache_victim
        cache_obj_t *main_cache_victim = main_cache->to_evict(main_cache, req);
        DEBUG_ASSERT(main_cache_victim != NULL);
        // if window_victim is more frequent, insert it into main_cache
        if (minimalIncrementCBF_estimate(params->CBF,
                                         (void *)&window_victim->obj_id,
                                         sizeof(window_victim->obj_id)) >
            minimalIncrementCBF_estimate(params->CBF,
                                         (void *)&main_cache_victim->obj_id,
                                         sizeof(main_cache_victim->obj_id))) {
#if defined(TRACK_DEMOTION)
          printf("%ld keep %ld %ld\n", cache->n_req, window_victim->create_time,
                 window_victim->next_access_vtime);
#endif

          main_cache->evict(main_cache, req);

          bool ret = window->remove(window, window_victim->obj_id);
          DEBUG_ASSERT(ret);

          cache_obj_t *cache_obj =
              main_cache->insert(main_cache, params->req_local);
          params->n_admit_bytes += params->req_local->obj_size;
          evicted = true;

        } else {
#if defined(TRACK_DEMOTION)
          printf("%ld demote %ld %ld\n", cache->n_req,
                 window_victim->create_time, window_victim->next_access_vtime);
#endif

          window->evict(window, req);
          evicted = true;
        }
      }
      // TODO @ Ziyue: add doorkeeper
      minimalIncrementCBF_add(params->CBF, (void *)(&params->req_local->obj_id),
                              sizeof(obj_id_t));
    } else {
      DEBUG_ASSERT(window->get_occupied_byte(window) == 0);
      main_cache->evict(main_cache, req);
      return;
    }
  }
}

static bool WTinyLFU_remove(cache_t *cache, obj_id_t obj_id) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)(cache->eviction_params);
  if (params->LRU->remove(params->LRU, obj_id)) {
    return true;
  }
  if (params->main_cache->remove(params->main_cache, obj_id)) {
    return true;
  }
  return false;
}

/* main_cache is only built after the parameters are parsed, so report the
 * configured type, which is what `-e print` runs against */
static const char *WTinyLFU_current_params(WTinyLFU_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "main-cache=%s,window-size=%.4lf,hill-climb=%d",
           params->main_cache == NULL ? params->main_cache_type
                                      : params->main_cache->cache_name,
           params->window_size, params->hill_climb);
  return params_str;
}

static void WTinyLFU_parse_params(cache_t *cache,
                                  const char *cache_specific_params) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)cache->eviction_params;

  // params->max_request_num = 32 * cache->cache_size; // 32 * cache_size

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *value = strsep(&params_str, ",");
    char *key = strsep(&value, "=");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    const char *error = NULL;
    if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", WTinyLFU_current_params(params));
      free(old_params_str);
      exit(0);
    } else if (value == NULL || *value == '\0') {
      error = "expected a nonempty value";
    } else if (strcasecmp(key, "main-cache") == 0) {
      strncpy(params->main_cache_type, value, 30);
    } else if (strcasecmp(key, "hill-climb") == 0) {
      if (strcmp(value, "0") != 0 && strcmp(value, "1") != 0) {
        error = "expected 0 or 1";
      } else {
        params->hill_climb = value[0] == '1';
      }
    } else if (strcasecmp(key, "window-size") == 0) {
      char *end;
      errno = 0;
      double window_size = strtod(value, &end);
      if (end == value || *end != '\0' ||
          (errno == ERANGE && window_size == 0) || !isfinite(window_size) ||
          window_size < 0 || window_size >= 1) {
        error = "expected a finite window-size in [0, 1)";
      } else {
        params->window_size = window_size;
      }
    } else {
      error = "unknown parameter";
    }
    if (error != NULL) {
      fprintf(stderr, "error: WTinyLFU parameter '%s': %s\n", key, error);
      free(old_params_str);
      exit(EXIT_FAILURE);
    }
  }
  free(old_params_str);
}

/* WTinyLFU cannot an object larger than segment size */
bool WTinyLFU_can_insert(cache_t *cache, const request_t *req) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);

  /* An object enters through the window, so the window's own per-object
   * overhead decides whether it fits there — not the main cache's, which can
   * differ. The main cache checks itself with its own overhead. */
  return can_insert &&
         (req->obj_size + params->LRU->obj_md_size <=
          params->LRU->cache_size) &&
         (params->main_cache->can_insert(params->main_cache, req));
}

static int64_t WTinyLFU_get_occupied_byte(const cache_t *cache) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;

  occupied_byte += params->LRU->occupied_byte;
  occupied_byte += params->main_cache->get_occupied_byte(params->main_cache);

  return occupied_byte;
}

static int64_t WTinyLFU_get_n_obj(const cache_t *cache) {
  WTinyLFU_params_t *params = (WTinyLFU_params_t *)cache->eviction_params;
  int64_t n_obj = 0;

  n_obj += params->LRU->n_obj;
  n_obj += params->main_cache->get_n_obj(params->main_cache);
  return n_obj;
}

#ifdef __cplusplus
}
#endif

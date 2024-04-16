//
//  multi handed clock
//  there are m hands, each points to i/m position of the clock
//  there is no action on hit,
//  on miss, one of the hands is selected to evict based on next access distance
//  (Belady) then the hands are reset to corresponding positions
//
//
//  mClock.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
  cache_obj_t **hands;
  int n_hands;
  double *hand_pos;
  int n_consider_obj;

  int last_eviction_hand;

  request_t *req_local;

} MClock_params_t;

static const char *DEFAULT_PARAMS =
    "hand-pos=0:0.25:0.50:0.75:1.0,n-consider-obj=1";
// static const char *DEFAULT_PARAMS =
// "hand-pos=0:0.10:0.20:0.30:0.40:0.50:0.60:0.70:0.80:0.90:1.0";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void MClock_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void MClock_free(cache_t *cache);
static bool MClock_get(cache_t *cache, const request_t *req);
static cache_obj_t *MClock_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *MClock_insert(cache_t *cache, const request_t *req);
static cache_obj_t *MClock_to_evict(cache_t *cache, const request_t *req);
static void MClock_evict(cache_t *cache, const request_t *req);
static bool MClock_remove(cache_t *cache, const obj_id_t obj_id);
static int64_t MClock_get_occupied_byte(const cache_t *cache);
static int64_t MClock_get_n_obj(const cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *MClock_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("MClock", ccache_params, cache_specific_params);
  cache->cache_init = MClock_init;
  cache->cache_free = MClock_free;
  cache->get = MClock_get;
  cache->find = MClock_find;
  cache->insert = MClock_insert;
  cache->evict = MClock_evict;
  cache->remove = MClock_remove;
  cache->to_evict = MClock_to_evict;
  cache->get_occupied_byte = MClock_get_occupied_byte;
  cache->get_n_obj = MClock_get_n_obj;
  cache->can_insert = cache_can_insert_default;
  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(MClock_params_t));
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  params->req_local = new_request();

  // we must parse the default params because the user specified params
  // may not have hand-pos and the hand_pos array will not be initialized
  MClock_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    MClock_parse_params(cache, cache_specific_params);
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "MClock-%d-%d",
           params->n_hands, params->n_consider_obj);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void MClock_free(cache_t *cache) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  free(params->hands);
  free(params->hand_pos);
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
 * @return true if cache hit, false if cache miss
 */
bool MClock_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *MClock_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  return cache_obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *MClock_insert(cache_t *cache, const request_t *req) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  if (params->hands[0] != NULL) {
    for (int i = 0; i < params->last_eviction_hand; i++) {
      params->hands[i] = params->hands[i]->queue.prev;
    }
    if (params->last_eviction_hand == params->n_hands - 1) {
      params->hands[params->last_eviction_hand] = params->q_tail;
    } else {
      params->hands[params->last_eviction_hand] =
          params->hands[params->last_eviction_hand]->queue.prev;
    }
    assert(params->hands[0] != NULL);
    assert(params->hands[0] == params->q_head);
  }

  return obj;
}

static void reset_hands(cache_t *cache) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  cache_obj_t *obj = params->q_head;
  int hand_idx = 0;
  int64_t obj_idx = 0;
  while (obj != NULL) {
    if (obj_idx >= params->hand_pos[hand_idx] * cache->n_obj) {
      params->hands[hand_idx++] = obj;
    }
    obj = obj->queue.next;
    obj_idx++;
  }
  if (params->hand_pos[params->n_hands - 1] == 1) {
    params->hands[hand_idx++] = params->q_tail;
  }
  assert(hand_idx == params->n_hands);
}

static void print_curr_hand_pos(cache_t *cache) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->q_head;
  int hand_idx = 0;
  int64_t obj_idx = 0;
  while (obj != NULL) {
    if (obj == params->hands[hand_idx]) {
      printf("hand %d: %ld %p %p %p\n", hand_idx, (long) obj_idx, obj, obj->queue.prev,
             obj->queue.next);
      hand_idx++;
    }
    obj = obj->queue.next;
    obj_idx++;
  }
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *MClock_to_evict(cache_t *cache, const request_t *req) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  int best_obj_pos = 0;
  cache_obj_t *best_obj = params->hands[0];
  double best_obj_benefit = best_obj->misc.next_access_vtime;
  for (int i = 0; i < params->n_hands; i++) {
    if (params->hands[i]->misc.next_access_vtime > best_obj_benefit) {
      best_obj_pos = i;
      best_obj = params->hands[i];
      best_obj_benefit = best_obj->misc.next_access_vtime;
    }
  }

  cache->to_evict_candidate_gen_vtime = cache->n_req;
  cache->to_evict_candidate = best_obj;
  assert(false);
  return best_obj;
}

static double calc_candidate_score1(cache_t *cache, cache_obj_t *obj) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  double score = 0;
  int n_score = 0;
  cache_obj_t *curr_obj = obj;

  for (int i = 0; i < params->n_consider_obj; i++) {
    if (curr_obj == NULL) {
      break;
    }
    score += 1.0 / (curr_obj->misc.next_access_vtime - cache->n_req);
    n_score++;
    curr_obj = curr_obj->queue.prev;
  }

  if (n_score > 0) score = n_score / score;
  return score;
}

static double calc_candidate_score(cache_t *cache, cache_obj_t *obj) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  double score = 0;
  int n_score = 0;
  cache_obj_t *curr_obj = obj;

  for (int i = 0; i < params->n_consider_obj; i++) {
    if (curr_obj == NULL) {
      break;
    }
    score += 1.0 / (curr_obj->misc.next_access_vtime - cache->n_req);
    n_score++;
    curr_obj = curr_obj->queue.prev;
  }

  if (n_score > 0) score = n_score / score;
  return score;
}
/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void MClock_evict(cache_t *cache, const request_t *req) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  static __thread int n_reset_hand = 0;
  if (params->hands[0] == NULL) {
    n_reset_hand++;
    assert(n_reset_hand < 2);
    reset_hands(cache);
  }

  int best_obj_pos = params->n_hands - 1;
  cache_obj_t *best_obj = params->hands[params->n_hands - 1];
  double score = calc_candidate_score(cache, best_obj);
  double best_score = score;

  /* 0.0 is the head (most recently inserted),
   * 1.0 is the tail (least recently inserted) */
  for (int i = params->n_hands - 1; i >= 0; i--) {
    score = calc_candidate_score(cache, params->hands[i]);
    if (score > best_score) {
      best_obj_pos = i;
      best_obj = params->hands[i];
      best_score = score;
    }
  }


  // static __thread int *n_choice_per_hand;
  // static __thread int n_evictions = 0;
  // if (n_choice_per_hand == NULL) {
  //   n_choice_per_hand = (int *)malloc(sizeof(int) * params->n_hands);
  //   memset(n_choice_per_hand, 0, sizeof(int) * params->n_hands);
  // }
  // n_choice_per_hand[best_obj_pos]++;
  // if (++n_evictions % 1000 == 0) {
  //   for (int i = 0; i < params->n_hands; i++) {
  //     printf("%d ", n_choice_per_hand[i]);
  //     n_choice_per_hand[i] = 0;
  //   }
  //   printf("\n");
  // }

  params->last_eviction_hand = best_obj_pos;
  params->hands[best_obj_pos] = best_obj->queue.next;
  DEBUG_ASSERT(best_obj_pos == params->n_hands - 1 ||
               best_obj->queue.next != NULL);

  remove_obj_from_list(&params->q_head, &params->q_tail, best_obj);
  cache_evict_base(cache, best_obj, true);
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
static bool MClock_remove(cache_t *cache, const obj_id_t obj_id) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  // need to update hands
  assert(false);
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);

  return true;
}

static int64_t MClock_get_occupied_byte(const cache_t *cache) {
  // MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  int64_t occupied_byte = cache->occupied_byte;
  return occupied_byte;
}

static int64_t MClock_get_n_obj(const cache_t *cache) {
  // MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  int64_t n_obj = cache->n_obj;
  return n_obj;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *MClock_current_params(cache_t *cache,
                                         MClock_params_t *params) {
  static __thread char params_str[128];
  int n = snprintf(params_str, 128, "hand-pos=%.2lf", (params->hand_pos[0]));

  for (int i = 1; i < params->n_hands; i++) {
    n += snprintf(params_str + n, 128 - n, ":%.2lf", (params->hand_pos[i]));
  }

  snprintf(cache->cache_name + n, 128 - n, "\n");

  return params_str;
}

static void MClock_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  MClock_params_t *params = (MClock_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "hand-pos") == 0) {
      int n_hands = 0;
#define MClock_MAX_N_HAND 16
      double hand_pos[MClock_MAX_N_HAND];
      char *v = strsep((char **)&value, ":");
      while (v != NULL) {
        hand_pos[n_hands++] = strtod(v, &end);
        v = strsep((char **)&value, ":");
      }
      params->n_hands = n_hands;
      if (params->hands != NULL) {
        free(params->hands);
      }
      if (params->hand_pos != NULL) {
        free(params->hand_pos);
      }
      params->hands = calloc(params->n_hands, sizeof(cache_obj_t *));
      params->hand_pos = calloc(params->n_hands, sizeof(double));
      for (int i = 0; i < n_hands; i++) {
        params->hand_pos[i] = hand_pos[i];
      }
      assert(hand_pos[0] == 0);
      assert(hand_pos[n_hands - 1] == 1);
    } else if (strcasecmp(key, "n-consider-obj") == 0) {
      params->n_consider_obj = (int)strtol(value, &end, 10);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", MClock_current_params(cache, params));
      exit(0);
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

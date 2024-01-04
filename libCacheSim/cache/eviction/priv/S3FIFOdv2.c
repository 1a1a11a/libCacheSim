//
//  10% small FIFO + 90% main FIFO (2-bit Clock) + ghost
//  insert to small FIFO if not in the ghost, else insert to the main FIFO
//  evict from small FIFO:
//      if object in the small is accessed,
//          reinsert to main FIFO,
//      else
//          evict and insert to the ghost
//  evict from main FIFO:
//      if object in the main is accessed,
//          reinsert to main FIFO,
//      else
//          evict
//
//
//  S3FIFOdv2.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"
#include "../../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROB_ARRAY_SIZE 10000000ULL
#define MAX_FREQ_THRESHOLD 8

#define MAX_FREQ_CNT 32

typedef struct {
  cache_t *fifo;
  cache_t *fifo_ghost;
  cache_t *main_cache;
  bool hit_on_ghost;

  int64_t n_obj_admit_to_fifo;
  int64_t n_obj_admit_to_main;
  int64_t n_obj_move_to_main;
  int64_t n_byte_admit_to_fifo;
  int64_t n_byte_admit_to_main;
  int64_t n_byte_move_to_main;

  int64_t n_obj_admit_to_fifo_last;
  int64_t n_obj_admit_to_main_last;
  int64_t n_obj_move_to_main_last;

  int32_t *first_access_cnt_over_age_fifo;
  int32_t *first_access_cnt_over_age_main;
  int64_t n_insertion;
  int32_t age_granularity;

  int move_to_main_threshold;
  int64_t eviction_freq_cnt[MAX_FREQ_THRESHOLD];
  int threshold_cnt[MAX_FREQ_THRESHOLD];

  int64_t main_freq_cnt[MAX_FREQ_CNT];
  int main_reinsert_threshold;

  int64_t small_freq_cnt[MAX_FREQ_CNT];
  int small_reinsert_threshold;

  double fifo_size_ratio;
  double ghost_size_ratio;
  char main_cache_type[32];

  request_t *req_local;
} S3FIFOdv2_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "fifo-size-ratio=0.10,ghost-size-ratio=0.90,move-to-main-threshold=1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *S3FIFOdv2_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);
static void S3FIFOdv2_free(cache_t *cache);
static bool S3FIFOdv2_get(cache_t *cache, const request_t *req);

static void S3FIFOdv2_rebalance(cache_t *cache);
static void S3FIFOdv2_choose_threshold(cache_t *cache);
static void S3FIFOdv2_update_main_reinsert_threshold(cache_t *cache);
static void S3FIFOdv2_update_main_reinsert_threshold_incorrect(cache_t *cache);
static void S3FIFOdv2_update_small_reinsert_threshold(cache_t *cache);

static cache_obj_t *S3FIFOdv2_find(cache_t *cache, const request_t *req,
                                   const bool update_cache);
static cache_obj_t *S3FIFOdv2_insert(cache_t *cache, const request_t *req);
static cache_obj_t *S3FIFOdv2_to_evict(cache_t *cache, const request_t *req);
static void S3FIFOdv2_evict(cache_t *cache, const request_t *req);
static bool S3FIFOdv2_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t S3FIFOdv2_get_occupied_byte(const cache_t *cache);
static inline int64_t S3FIFOdv2_get_n_obj(const cache_t *cache);
static inline bool S3FIFOdv2_can_insert(cache_t *cache, const request_t *req);
static void S3FIFOdv2_parse_params(cache_t *cache,
                                   const char *cache_specific_params);

static void S3FIFOdv2_evict_fifo(cache_t *cache, const request_t *req);
static void S3FIFOdv2_evict_main(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *S3FIFOdv2_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("S3FIFOdv2", ccache_params, cache_specific_params);
  cache->cache_init = S3FIFOdv2_init;
  cache->cache_free = S3FIFOdv2_free;
  cache->get = S3FIFOdv2_get;
  cache->find = S3FIFOdv2_find;
  cache->insert = S3FIFOdv2_insert;
  cache->evict = S3FIFOdv2_evict;
  cache->remove = S3FIFOdv2_remove;
  cache->to_evict = S3FIFOdv2_to_evict;
  cache->get_n_obj = S3FIFOdv2_get_n_obj;
  cache->get_occupied_byte = S3FIFOdv2_get_occupied_byte;
  cache->can_insert = S3FIFOdv2_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(S3FIFOdv2_params_t));
  memset(cache->eviction_params, 0, sizeof(S3FIFOdv2_params_t));
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  params->first_access_cnt_over_age_fifo =
      malloc(sizeof(int32_t) * PROB_ARRAY_SIZE);
  params->first_access_cnt_over_age_main =
      malloc(sizeof(int32_t) * PROB_ARRAY_SIZE);
  memset(params->first_access_cnt_over_age_fifo, 0,
         sizeof(int32_t) * PROB_ARRAY_SIZE);
  memset(params->first_access_cnt_over_age_main, 0,
         sizeof(int32_t) * PROB_ARRAY_SIZE);

  params->age_granularity =
      (int32_t)ceil((double)ccache_params.cache_size * 2 / PROB_ARRAY_SIZE);

  params->req_local = new_request();
  params->hit_on_ghost = false;
  memset(params->main_freq_cnt, 0, sizeof(int64_t) * MAX_FREQ_CNT);
  params->main_reinsert_threshold = 1;
  memset(params->small_freq_cnt, 0, sizeof(int64_t) * MAX_FREQ_CNT);
  params->small_reinsert_threshold = 1;

  S3FIFOdv2_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    S3FIFOdv2_parse_params(cache, cache_specific_params);
  }

  int64_t fifo_cache_size =
      (int64_t)ccache_params.cache_size * params->fifo_size_ratio;
  int64_t main_cache_size = ccache_params.cache_size - fifo_cache_size;
  int64_t fifo_ghost_cache_size =
      (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = fifo_cache_size;
  params->fifo = FIFO_init(ccache_params_local, NULL);

  if (fifo_ghost_cache_size > 0) {
    ccache_params_local.cache_size = fifo_ghost_cache_size;
    params->fifo_ghost = FIFO_init(ccache_params_local, NULL);
    snprintf(params->fifo_ghost->cache_name, CACHE_NAME_ARRAY_LEN,
             "FIFO-ghost");
  } else {
    params->fifo_ghost = NULL;
  }

  ccache_params_local.cache_size = main_cache_size;
  params->main_cache = FIFO_init(ccache_params_local, NULL);

#if defined(TRACK_EVICTION_V_AGE)
  if (params->fifo_ghost != NULL) {
    params->fifo_ghost->track_eviction_age = false;
  }
  params->fifo->track_eviction_age = false;
  params->main_cache->track_eviction_age = false;
#endif

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFOdv2-%.4lf-%d",
           params->fifo_size_ratio, params->move_to_main_threshold);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void S3FIFOdv2_free(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->fifo->cache_free(params->fifo);
  if (params->fifo_ghost != NULL) {
    params->fifo_ghost->cache_free(params->fifo_ghost);
  }
  params->main_cache->cache_free(params->main_cache);
  free(cache->eviction_params);
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
static bool S3FIFOdv2_get(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->fifo->get_occupied_byte(params->fifo) +
                   params->main_cache->get_occupied_byte(params->main_cache) <=
               cache->cache_size);

  bool cache_hit = cache_get_base(cache, req);

  if (params->main_cache->get_occupied_byte(params->main_cache) >
          0.9 * params->main_cache->cache_size &&
      cache->n_req % 1000 == 0) {
    // S3FIFOdv2_rebalance(cache);
    // S3FIFOdv2_choose_threshold(cache);
    S3FIFOdv2_update_main_reinsert_threshold(cache);
    S3FIFOdv2_update_main_reinsert_threshold_incorrect(cache);
    // S3FIFOdv2_update_small_reinsert_threshold(cache);
  }

  // if (cache->n_req % 1000000 == 0) {
  //   double s = params->n_obj_admit_to_fifo + params->n_obj_admit_to_main +
  //              params->n_obj_move_to_main;
  //   printf("\n#################### %ld %ld + %ld = %ld %d\n", cache->n_req,
  //          params->fifo->cache_size, params->main_cache->cache_size,
  //          cache->cache_size, params->move_to_main_threshold);
  //   printf("%ld %ld: %.4lf/%.4lf/%.4lf\n", cache->n_req, cache->cache_size,
  //          (double)params->n_obj_admit_to_fifo / s,
  //          (double)params->n_obj_admit_to_main / s,
  //          (double)params->n_obj_move_to_main / s);
  // }

  return cache_hit;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
static void S3FIFOdv2_update_main_reinsert_threshold_incorrect(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  int n_main_obj = params->main_cache->get_n_obj(params->main_cache);
  double inserttion_frac = 0.1;
  int64_t n_drop_obj = n_main_obj * (1 - inserttion_frac);
  int64_t n_obj = 0;

  const int64_t POWER2_MAPPING[] = {
      1,         2,         4,         8,        16,       32,       64,
      128,       256,       512,       1024,     2048,     4096,     8192,
      16384,     32768,     65536,     131072,   262144,   524288,   1048576,
      2097152,   4194304,   8388608,   16777216, 33554432, 67108864, 134217728,
      268435456, 536870912, 1073741824};

  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->main_freq_cnt[i];
    if (n_obj > n_drop_obj) {
      params->move_to_main_threshold = POWER2_MAPPING[i];
      break;
    }
  }

  n_obj = 0;
  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->main_freq_cnt[i];
  }
  if (n_obj != n_main_obj)
    ERROR("n_obj %ld != n_main_obj %d\n", (long)n_obj, n_main_obj);

  // if (params->move_to_main_threshold > 1) {
  //   printf("set threshold to %d, ", params->move_to_main_threshold);
  //   for (int i = 0; i < 8; i++) {
  //     printf("%ld ", params->main_freq_cnt[i]);
  //   }
  //   printf("\n");
  // }
}

static void S3FIFOdv2_update_main_reinsert_threshold(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  int n_main_obj = params->main_cache->get_n_obj(params->main_cache);
  double inserttion_frac = 0.1;
  int64_t n_drop_obj = n_main_obj * (1 - inserttion_frac);
  int64_t n_obj = 0;

  const int64_t POWER2_MAPPING[] = {
      1,         2,         4,         8,        16,       32,       64,
      128,       256,       512,       1024,     2048,     4096,     8192,
      16384,     32768,     65536,     131072,   262144,   524288,   1048576,
      2097152,   4194304,   8388608,   16777216, 33554432, 67108864, 134217728,
      268435456, 536870912, 1073741824};

  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->main_freq_cnt[i];
    if (n_obj > n_drop_obj) {
      params->main_reinsert_threshold = POWER2_MAPPING[i];
      break;
    }
  }
  // printf("%ld %ld %ld %ld - %ld/%d\n", params->main_freq_cnt[0],
  //        params->main_freq_cnt[1], params->main_freq_cnt[2],
  //        params->main_freq_cnt[3], n_drop_obj, n_main_obj);

  n_obj = 0;
  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->main_freq_cnt[i];
  }
  if (n_obj != n_main_obj)
    ERROR("n_obj %ld != n_main_obj %d\n", (long)n_obj, n_main_obj);

  // if (params->main_reinsert_threshold > 1) {
  //   printf("set threshold to %d, ", params->main_reinsert_threshold);
  //   for (int i = 0; i < 8; i++) {
  //     printf("%ld ", params->main_freq_cnt[i]);
  //   }
  //   printf("\n");
  // }
}

static void S3FIFOdv2_update_small_reinsert_threshold(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  int n_small_obj = params->fifo->get_n_obj(params->fifo);
  double inserttion_frac = 0.2;
  int64_t n_drop_obj = n_small_obj * (1 - inserttion_frac);
  int64_t n_obj = 0;

  const int64_t POWER2_MAPPING[] = {
      1,         2,         4,         8,        16,       32,       64,
      128,       256,       512,       1024,     2048,     4096,     8192,
      16384,     32768,     65536,     131072,   262144,   524288,   1048576,
      2097152,   4194304,   8388608,   16777216, 33554432, 67108864, 134217728,
      268435456, 536870912, 1073741824};

  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->small_freq_cnt[i];
    if (n_obj > n_drop_obj) {
      params->move_to_main_threshold = POWER2_MAPPING[i];
      break;
    }
  }
  // printf("%ld %ld %ld %ld - %ld/%ld\n", params->small_freq_cnt[0],
  //        params->small_freq_cnt[1], params->small_freq_cnt[2],
  //        params->small_freq_cnt[3], n_drop_obj, n_small_obj);

  n_obj = 0;
  for (int i = 0; i < MAX_FREQ_CNT; i++) {
    n_obj += params->small_freq_cnt[i];
  }
  if (n_obj != n_small_obj)
    ERROR("n_obj %ld != n_small_obj %d\n", (long)n_obj, n_small_obj);

  // if (params->move_to_main_threshold > 1) {
  //   printf("set threshold to %d, ", params->move_to_main_threshold);
  //   for (int i = 0; i < 8; i++) {
  //     printf("%ld ", params->small_freq_cnt[i]);
  //   }
  //   printf("\n");
  // }
}

static void S3FIFOdv2_choose_threshold(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  int64_t sum_cnt = 0;
  for (int i = 0; i < MAX_FREQ_THRESHOLD; i++) {
    sum_cnt += params->eviction_freq_cnt[i];
  }

  if (sum_cnt < 10000) {
    return;
  }

  int64_t sum_cnt2 = sum_cnt;
  for (int i = 0; i < MAX_FREQ_THRESHOLD; i++) {
    sum_cnt2 -= params->eviction_freq_cnt[i];
    if (sum_cnt2 < sum_cnt / 5) {
      params->move_to_main_threshold = i + 1;
      break;
    }
  }

  params->threshold_cnt[params->move_to_main_threshold - 1]++;
  for (int i = 0; i < MAX_FREQ_THRESHOLD; i++) {
    params->eviction_freq_cnt[i] /= 2;
  }

  if (params->move_to_main_threshold != 1) {
    for (int i = 0; i < MAX_FREQ_THRESHOLD; i++) {
      printf("%8ld/%-4d ", (long)params->eviction_freq_cnt[i],
             (int)params->threshold_cnt[i]);
    }

    printf("move_to_main_threshold: %d\n", params->move_to_main_threshold);
  }
}

static bool S3FIFOdv2_rebalance0(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;

  int64_t sum_fifo = 0;
  int64_t sum_main = 0;
  for (int i = 0; i < PROB_ARRAY_SIZE; i++) {
    sum_fifo += params->first_access_cnt_over_age_fifo[i];
    sum_main += params->first_access_cnt_over_age_main[i];
  }

  int64_t sum_one_perc_fifo = 0, sum_one_perc_main = 0;
  size_t pos_fifo = params->fifo->cache_size / params->age_granularity;
  size_t pos_main = params->main_cache->cache_size / params->age_granularity;
  size_t n_slots =
      MIN(params->fifo->cache_size, params->main_cache->cache_size) / 20 + 1;
  for (size_t i = 0; i < n_slots; i++) {
    sum_one_perc_fifo += params->first_access_cnt_over_age_fifo[pos_fifo - i];
    sum_one_perc_main += params->first_access_cnt_over_age_main[pos_main - i];
  }

  if (sum_one_perc_fifo + sum_one_perc_main < 100) {
    return false;
  }

  // for (int i = 0; i < 10; i++) {
  //   printf("%4d ", params->first_access_cnt_over_age_fifo[i]);
  // }
  // printf("%ld - %ld\n", sum_fifo, sum_one_perc_fifo);

  // for (int i = 0; i < 10; i++) {
  //   printf("%4d ", params->first_access_cnt_over_age_main[i]);
  // }
  // printf("%ld - %ld\n", sum_main, sum_one_perc_main);

  n_slots = n_slots / 5;
  if (sum_one_perc_fifo > sum_one_perc_main * 2) {
    params->fifo->cache_size += n_slots;
    params->main_cache->cache_size -= n_slots;
    params->fifo_ghost->cache_size -= n_slots;
    // printf("move to fifo %ld\n", n_slots);
  } else if (sum_one_perc_main > sum_one_perc_fifo * 2) {
    params->fifo->cache_size -= n_slots;
    params->main_cache->cache_size += n_slots;
    params->fifo_ghost->cache_size += n_slots;
    // printf("move to main %ld\n", n_slots);
  }

  memset(params->first_access_cnt_over_age_fifo, 0,
         sizeof(*params->first_access_cnt_over_age_fifo));
  memset(params->first_access_cnt_over_age_main, 0,
         sizeof(*params->first_access_cnt_over_age_main));

  return true;
}

static void S3FIFOdv2_rebalance(cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;

  // double s = params->n_obj_admit_to_fifo + params->n_obj_admit_to_main +
  //            params->n_obj_move_to_main;

  size_t n_slots =
      MIN(params->fifo->cache_size, params->main_cache->cache_size) / 100 + 1;

  if ((double)(params->n_obj_admit_to_main - params->n_obj_admit_to_main_last) /
          (params->n_obj_admit_to_fifo - params->n_obj_admit_to_fifo_last) >
      0.1) {
    if ((double)(params->fifo->cache_size) / (double)(cache->cache_size) >
            0.10 &&
        params->move_to_main_threshold > 1) {
      params->move_to_main_threshold -= 1;
    }
    params->fifo->cache_size += n_slots;
    params->main_cache->cache_size -= n_slots;
    params->fifo_ghost->cache_size -= n_slots;
  }
  if ((double)(params->n_obj_move_to_main - params->n_obj_move_to_main_last) /
          (params->n_obj_admit_to_fifo - params->n_obj_admit_to_fifo_last) >
      0.1) {
    if ((double)(params->fifo->cache_size) / (double)(cache->cache_size) <
        0.001) {
      params->move_to_main_threshold += 1;
    }
    params->fifo->cache_size -= n_slots;
    params->main_cache->cache_size += n_slots;
    params->fifo_ghost->cache_size += n_slots;
  }

  params->n_obj_admit_to_fifo_last = params->n_obj_admit_to_fifo;
  params->n_obj_admit_to_main_last = params->n_obj_admit_to_main;
  params->n_obj_move_to_main_last = params->n_obj_move_to_main;
}

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
static cache_obj_t *S3FIFOdv2_find(cache_t *cache, const request_t *req,
                                   const bool update_cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;

  // if update cache is false, we only check the fifo and main caches
  if (!update_cache) {
    cache_obj_t *obj = params->fifo->find(params->fifo, req, false);
    if (obj != NULL) {
      return obj;
    }
    obj = params->main_cache->find(params->main_cache, req, false);
    if (obj != NULL) {
      return obj;
    }
    return NULL;
  }

  /* update cache is true from now */
  params->hit_on_ghost = false;
  cache_obj_t *obj = params->fifo->find(params->fifo, req, true);
  if (obj != NULL) {
    obj->S3FIFO.freq += 1;
    size_t pos1 = log2_ull(obj->S3FIFO.freq - 1);
    size_t pos2 = log2_ull(obj->S3FIFO.freq);
    params->small_freq_cnt[pos1] -= 1;  // remove from old freq
    params->small_freq_cnt[pos2] += 1;  // add to new freq

    if (obj->misc.freq == 1) {
      size_t insertion_age = params->n_insertion - obj->S3FIFO.insertion_time;
      if (insertion_age / params->age_granularity >= PROB_ARRAY_SIZE) {
        insertion_age = PROB_ARRAY_SIZE - 1;
      }
      params->first_access_cnt_over_age_fifo[insertion_age /
                                             params->age_granularity] += 1;
    }
    return obj;
  }

  if (params->fifo_ghost != NULL &&
      params->fifo_ghost->remove(params->fifo_ghost, req->obj_id)) {
    // if object in fifo_ghost, remove will return true
    params->hit_on_ghost = true;
  }

  obj = params->main_cache->find(params->main_cache, req, true);
  if (obj != NULL) {
    obj->S3FIFO.freq += 1;
    size_t pos1 = log2_ull(obj->S3FIFO.freq - 1);
    size_t pos2 = log2_ull(obj->S3FIFO.freq);
    params->main_freq_cnt[pos1] -= 1;  // remove from old freq
    params->main_freq_cnt[pos2] += 1;  // add to new freq

    // this wont happen
    if (obj->misc.freq - obj->S3FIFO.main_insert_freq == 1) {
      size_t insertion_age = params->n_insertion - obj->S3FIFO.insertion_time;
      if (insertion_age / params->age_granularity >= PROB_ARRAY_SIZE) {
        insertion_age = PROB_ARRAY_SIZE - 1;
      }
      params->first_access_cnt_over_age_main[insertion_age /
                                             params->age_granularity] += 1;
    }
  }

  return obj;
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
static cache_obj_t *S3FIFOdv2_insert(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    /* insert into the ARC */
    params->hit_on_ghost = false;
    params->n_obj_admit_to_main += 1;
    params->n_byte_admit_to_main += req->obj_size;
    obj = params->main_cache->insert(params->main_cache, req);
    obj->S3FIFO.main_insert_freq = 0;
    params->main_freq_cnt[0] += 1;
  } else {
    /* insert into the fifo */
    if (req->obj_size >= params->fifo->cache_size) {
      return NULL;
    }
    params->n_obj_admit_to_fifo += 1;
    params->n_byte_admit_to_fifo += req->obj_size;
    obj = params->fifo->insert(params->fifo, req);
    params->small_freq_cnt[0] += 1;
  }

  obj->S3FIFO.insertion_time = params->n_insertion++;

#if defined(TRACK_EVICTION_V_AGE)
  obj->create_time = CURR_TIME(cache, req);
#endif

#if defined(TRACK_DEMOTION)
  obj->create_time = cache->n_req;
#endif

  obj->S3FIFO.freq = 0;

  return obj;
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
static cache_obj_t *S3FIFOdv2_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
  return NULL;
}

static void S3FIFOdv2_evict_fifo(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  cache_t *fifo = params->fifo;
  cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  bool has_evicted = false;
  while (!has_evicted && fifo->get_occupied_byte(fifo) > 0) {
    // evict from FIFO
    cache_obj_t *obj_to_evict = fifo->to_evict(fifo, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    // need to copy the object before it is evicted
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    size_t pos = MIN(obj_to_evict->S3FIFO.freq, MAX_FREQ_THRESHOLD - 1);
    params->eviction_freq_cnt[pos] += 1;

    params->small_freq_cnt[log2_ull(obj_to_evict->S3FIFO.freq)] -= 1;

    if (obj_to_evict->S3FIFO.freq >= params->move_to_main_threshold) {
#if defined(TRACK_DEMOTION)
      printf("%ld keep %ld %ld\n", cache->n_req, obj_to_evict->create_time,
             obj_to_evict->misc.next_access_vtime);
#endif
      // freq is updated in cache_find_base
      params->n_obj_move_to_main += 1;
      params->n_byte_move_to_main += obj_to_evict->obj_size;

      cache_obj_t *new_obj = main->insert(main, params->req_local);
      new_obj->misc.freq = obj_to_evict->misc.freq;
      new_obj->S3FIFO.freq = 0;
      new_obj->S3FIFO.main_insert_freq = obj_to_evict->misc.freq;
      params->main_freq_cnt[0] += 1;
#if defined(TRACK_EVICTION_V_AGE)
      new_obj->create_time = obj_to_evict->create_time;
    } else {
      record_eviction_age(cache, obj_to_evict,
                          CURR_TIME(cache, req) - obj_to_evict->create_time);
#else
    } else {
#endif

#if defined(TRACK_DEMOTION)
      printf("%ld demote %ld %ld\n", cache->n_req, obj_to_evict->create_time,
             obj_to_evict->misc.next_access_vtime);
#endif

      // insert to ghost
      if (ghost != NULL) {
        ghost->get(ghost, params->req_local);
      }
      has_evicted = true;
    }

    // remove from fifo, but do not update stat
    bool removed = fifo->remove(fifo, params->req_local->obj_id);
    assert(removed);
  }
}

static void S3FIFOdv2_evict_main(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  // cache_t *fifo = params->fifo;
  // cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  // evict from main cache
  bool has_evicted = false;
  while (!has_evicted && main->get_occupied_byte(main) > 0) {
    cache_obj_t *obj_to_evict = main->to_evict(main, req);
    DEBUG_ASSERT(obj_to_evict != NULL);
    int freq = obj_to_evict->S3FIFO.freq;
    int total_freq = obj_to_evict->misc.freq;
#if defined(TRACK_EVICTION_V_AGE)
    int64_t create_time = obj_to_evict->create_time;
#endif
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    params->main_freq_cnt[log2_ull(freq)] -= 1;
    if (freq >= params->main_reinsert_threshold) {
      // we need to evict first because the object to insert has the same obj_id
      main->remove(main, obj_to_evict->obj_id);
      obj_to_evict = NULL;

      cache_obj_t *new_obj = main->insert(main, params->req_local);
      // clock with 2-bit counter
      new_obj->S3FIFO.freq = MIN(freq, 3) - 1;
      new_obj->misc.freq = total_freq;
      params->main_freq_cnt[log2_ull(new_obj->S3FIFO.freq)] += 1;

#if defined(TRACK_EVICTION_V_AGE)
      new_obj->create_time = create_time;
#endif
    } else {
#if defined(TRACK_EVICTION_V_AGE)
      record_eviction_age(cache, obj_to_evict,
                          CURR_TIME(cache, req) - obj_to_evict->create_time);
#endif

      // main->evict(main, req);
      bool removed = main->remove(main, obj_to_evict->obj_id);
      if (!removed) {
        ERROR("cannot remove obj %ld\n", (long)obj_to_evict->obj_id);
      }

      has_evicted = true;
    }
  }
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
static void S3FIFOdv2_evict(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;

  cache_t *fifo = params->fifo;
  // cache_t *ghost = params->fifo_ghost;
  cache_t *main = params->main_cache;

  if (main->get_occupied_byte(main) > main->cache_size ||
      fifo->get_occupied_byte(fifo) == 0) {
    return S3FIFOdv2_evict_main(cache, req);
  }
  return S3FIFOdv2_evict_fifo(cache, req);
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
static bool S3FIFOdv2_remove(cache_t *cache, const obj_id_t obj_id) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->fifo->remove(params->fifo, obj_id);
  removed = removed || (params->fifo_ghost &&
                        params->fifo_ghost->remove(params->fifo_ghost, obj_id));
  removed = removed || params->main_cache->remove(params->main_cache, obj_id);

  return removed;
}

static inline int64_t S3FIFOdv2_get_occupied_byte(const cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  return params->fifo->get_occupied_byte(params->fifo) +
         params->main_cache->get_occupied_byte(params->main_cache);
}

static inline int64_t S3FIFOdv2_get_n_obj(const cache_t *cache) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;
  return params->fifo->get_n_obj(params->fifo) +
         params->main_cache->get_n_obj(params->main_cache);
}

static inline bool S3FIFOdv2_can_insert(cache_t *cache, const request_t *req) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)cache->eviction_params;

  return req->obj_size <= params->fifo->cache_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *S3FIFOdv2_current_params(S3FIFOdv2_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-size-ratio=%.4lf,main-cache=%s\n",
           params->fifo_size_ratio, params->main_cache->cache_name);
  return params_str;
}

static void S3FIFOdv2_parse_params(cache_t *cache,
                                   const char *cache_specific_params) {
  S3FIFOdv2_params_t *params = (S3FIFOdv2_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  // char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "fifo-size-ratio") == 0) {
      params->fifo_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", S3FIFOdv2_current_params(params));
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

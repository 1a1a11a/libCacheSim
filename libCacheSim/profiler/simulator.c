//
//  profiler.c
//  libCacheSim
//
//  Created by Juncheng on 11/20/19.
//  Copyright © 2016-2019 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/simulator.h"

#include <math.h>

#include "../cache/cacheUtils.h"
#include "../include/libCacheSim/evictionAlgo.h"
#include "../include/libCacheSim/plugin.h"
#include "../utils/include/myprint.h"
#include "../utils/include/mystr.h"

typedef struct simulator_multithreading_params {
  reader_t *reader;
  ssize_t n_caches;
  cache_t **caches;
  uint64_t n_warmup_req; /* num of requests used for warming up cache */
  reader_t *warmup_reader;
  int warmup_sec; /* num of seconds of requests used for warming up cache */
  cache_stat_t *result;
  GMutex mtx; /* prevent simultaneous write to progress */
  gint *progress;
  gpointer other_data;
} sim_mt_params_t;

static void _simulate(gpointer data, gpointer user_data) {
  sim_mt_params_t *params = (sim_mt_params_t *)user_data;
  int idx = GPOINTER_TO_UINT(data) - 1;
  set_rand_seed(0);

  cache_stat_t *result = params->result;
  reader_t *cloned_reader = clone_reader(params->reader);
  request_t *req = new_request();
  cache_t *local_cache = params->caches[idx];
  // cache_t *local_cache =
  //     create_cache_with_new_size(params->cache, result[idx].cache_size);

  /* warm up using warmup_reader */
  if (params->warmup_reader) {
    reader_t *warmup_cloned_reader = clone_reader(params->warmup_reader);
    read_one_req(warmup_cloned_reader, req);
    while (req->valid) {
      cache_ck_res_e ck = local_cache->get(local_cache, req);
      local_cache->stat.n_warmup_req += 1;
      read_one_req(warmup_cloned_reader, req);
    }
    close_reader(warmup_cloned_reader);
    INFO("cache %s (size %" PRIu64
         ") finishes warm up using warmup reader "
         "with %" PRIu64 "requests\n",
         local_cache->cache_name, local_cache->cache_size,
         local_cache->stat.n_warmup_req);
  }

  read_one_req(cloned_reader, req);
  int64_t start_ts = (int64_t)req->real_time;

  /* using warmup_frac or warmup_sec of requests from reader to warm up */
  if (params->n_warmup_req > 0 || params->warmup_sec > 0) {
    uint64_t n_warmup = 0;
    while (req->valid && (n_warmup < params->n_warmup_req ||
                          req->real_time - start_ts < params->warmup_sec)) {
      req->real_time -= start_ts;
      cache_ck_res_e ck = local_cache->get(local_cache, req);
      n_warmup += 1;
      read_one_req(cloned_reader, req);
    }
    local_cache->stat.n_warmup_req += n_warmup;
    INFO("cache %s (size %" PRIu64
         ") finishes warm up using "
         "with %" PRIu64 " requests, %.2lf hour trace time\n",
         local_cache->cache_name, local_cache->cache_size, n_warmup,
         (double)(req->real_time - start_ts) / 3600.0);
  }

  while (req->valid) {
    local_cache->stat.n_req++;
    local_cache->stat.n_req_byte += req->obj_size;

    req->real_time -= start_ts;
    if (local_cache->get(local_cache, req) != cache_ck_hit) {
      local_cache->stat.n_miss++;
      local_cache->stat.n_miss_byte += req->obj_size;
    }
    read_one_req(cloned_reader, req);
  }

  /* get expiration information */
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (local_cache->hashtable->n_obj != 0) {
    cache_stat_t stat;
    memset(&stat, 0, sizeof(cache_stat_t));
    stat.curr_rtime = req->real_time;
    get_cache_state(local_cache, &stat);
    assert(local_cache->occupied_size == stat.occupied_size);
    assert(local_cache->n_obj == stat.n_obj);
    local_cache->stat.curr_rtime = req->real_time;
    local_cache->stat.expired_obj_cnt = stat.expired_obj_cnt;
    local_cache->stat.expired_bytes = stat.expired_bytes;
  }
#endif

  local_cache->stat.n_obj = local_cache->n_obj;
  local_cache->stat.occupied_size = local_cache->occupied_size;
  result[idx] = local_cache->stat;

  // report progress
  g_mutex_lock(&(params->mtx));
  (*(params->progress))++;
  g_mutex_unlock(&(params->mtx));

  // clean up
  free_request(req);
  close_reader(cloned_reader);
  // local_cache->cache_free(local_cache);
}

cache_stat_t *simulate_at_multi_sizes_with_step_size(
    reader_t *const reader, const cache_t *cache, uint64_t step_size,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads) {
  int num_of_sizes = (int)ceil((double)cache->cache_size / (double)step_size);
  get_num_of_req(reader);
  uint64_t *cache_sizes = my_malloc_n(uint64_t, num_of_sizes);
  for (int i = 0; i < num_of_sizes; i++) {
    cache_sizes[i] = step_size * (i + 1);
  }

  cache_stat_t *res = simulate_at_multi_sizes(
      reader, cache, num_of_sizes, cache_sizes, warmup_reader, warmup_frac,
      warmup_sec, num_of_threads);
  my_free(sizeof(uint64_t) * num_of_sizes, cache_sizes);
  return res;
}

/**
 * @brief get miss ratio curve for different cache sizes
 *
 * @param reader
 * @param cache
 * @param num_of_sizes
 * @param cache_sizes
 * @param warmup_reader if not NULL, read from warmup_reader to warm up cache
 * @param warmup_frac use warmup_frac of requests from reader to warm up cache
 * @param warmup_sec uses warmup_sec seconds of requests to warm up cache
 * @param num_of_threads
 *
 * note that warmup_reader, warmup_frac and warmup_sec are mutually exclusive
 *
 */
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache,
                                      int num_of_sizes,
                                      const uint64_t *cache_sizes,
                                      reader_t *warmup_reader,
                                      double warmup_frac, int warmup_sec,
                                      int num_of_threads) {
  int progress = 0;

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_sizes);

  // build parameters and send to thread pool
  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->reader = reader;
  params->warmup_reader = warmup_reader;
  params->warmup_sec = warmup_sec;
  params->n_caches = num_of_sizes;
  params->n_warmup_req =
      (uint64_t)((double)get_num_of_req(reader) * warmup_frac);
  params->result = result;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));

  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new(
      (GFunc)_simulate, (gpointer)params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  // start computation
  params->caches = my_malloc_n(cache_t *, num_of_sizes);
  for (int i = 1; i < num_of_sizes + 1; i++) {
    params->caches[i - 1] =
        create_cache_with_new_size(cache, cache_sizes[i - 1]);
    result[i - 1].cache_size = cache_sizes[i - 1];
    strncpy(result[i - 1].cache_name, cache->cache_name, MAX_CACHE_NAME_LEN);

    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(cache_sizes[0], start_cache_size);
  convert_size_to_str(cache_sizes[num_of_sizes - 1], end_cache_size);

  INFO(
      "%s starts computation %s, num_warmup_req %lld, start cache size %s, "
      "end cache size %s, %d sizes, %d threads, please wait\n",
      __func__, cache->cache_name, (long long)(params->n_warmup_req),
      start_cache_size, end_cache_size, num_of_sizes, num_of_threads);

  // wait for all simulations to finish
  while (progress < (uint64_t)num_of_sizes - 1) {
    print_progress((double)progress / (double)(num_of_sizes - 1) * 100);
  }

  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  for (int i = 0; i < num_of_sizes; i++) {
    params->caches[i]->cache_free(params->caches[i]);
  }
  my_free(sizeof(cache_t *) * num_of_sizes, params->caches);
  my_free(sizeof(sim_mt_params_t), params);

  printf("%s\n", result[0].cache_name);
  // user is responsible for free-ing the result
  return result;
}

/**
 * @brief run multiple simulations in parallel
 *
 * @param reader
 * @param caches
 * @param num_of_caches
 * @param warmup_reader
 * @param warmup_frac
 * @param warmup_sec
 * @param num_of_threads
 * @return cache_stat_t*
 */
cache_stat_t *simulate_with_multi_caches(reader_t *reader, cache_t *caches[],
                                         int num_of_caches,
                                         reader_t *warmup_reader,
                                         double warmup_frac, int warmup_sec,
                                         int num_of_threads) {
  int i, progress = 0;

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_caches);

  // build parameters and send to thread pool
  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->reader = reader;
  params->caches = caches;
  params->warmup_reader = warmup_reader;
  params->warmup_sec = warmup_sec;
  params->n_warmup_req =
      (uint64_t)((double)get_num_of_req(reader) * warmup_frac);
  params->result = result;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));

  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new(
      (GFunc)_simulate, (gpointer)params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  // start computation
  for (i = 1; i < num_of_caches + 1; i++) {
    result[i - 1].cache_size = caches[i - 1]->cache_size;
    strncpy(result[i - 1].cache_name, caches[i - 1]->cache_name,
            MAX_CACHE_NAME_LEN);

    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(result[0].cache_size, start_cache_size);
  convert_size_to_str(result[num_of_caches - 1].cache_size, end_cache_size);

  INFO(
      "%s starts computation, num_warmup_req %lld, start cache %s size %s, "
      "end cache %s size %s, %d sizes, %d threads, please wait\n",
      __func__, (long long)(params->n_warmup_req), caches[0]->cache_name,
      start_cache_size, caches[num_of_caches - 1]->cache_name, end_cache_size,
      num_of_caches, num_of_threads);

  // wait for all simulations to finish
  while (progress < (uint64_t)num_of_caches - 1) {
    print_progress((double)progress / (double)(num_of_caches - 1) * 100);
  }

  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  my_free(sizeof(sim_mt_params_t), params);

  // user is responsible for free-ing the result
  return result;
}

#ifdef __cplusplus
}
#endif

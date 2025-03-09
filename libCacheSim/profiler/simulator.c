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
  reader_t **readers;
  ssize_t n_caches;
  cache_t **caches;
  uint64_t n_warmup_req; /* num of requests used for warming up cache */
  reader_t *warmup_reader;
  int warmup_sec; /* num of seconds of requests used for warming up cache */
  cache_stat_t *result;
  GMutex mtx; /* prevent simultaneous write to progress */
  gint *progress;
  gpointer other_data;
  bool free_cache_when_finish;
  bool use_random_seed;
} sim_mt_params_t;

static void _simulate(gpointer data, gpointer user_data) {
  sim_mt_params_t *params = (sim_mt_params_t *)user_data;
  int idx = GPOINTER_TO_UINT(data) - 1;
  if (params->use_random_seed) {
    set_rand_seed(rand());
  } else {
    set_rand_seed(1);
  }

  cache_stat_t *result = params->result;
  /* If an array of readers is provided, use the one corresponding to this cache; otherwise, use the single reader */
  reader_t *source_reader = params->readers ? params->readers[idx] : params->reader;
  reader_t *cloned_reader = clone_reader(source_reader);
  request_t *req = new_request();
  cache_t *local_cache = params->caches[idx];
  strncpy(result[idx].cache_name, local_cache->cache_name, CACHE_NAME_ARRAY_LEN);

  /* warm up using warmup_reader */
  if (params->warmup_reader) {
    reader_t *warmup_cloned_reader = clone_reader(params->warmup_reader);
    read_one_req(warmup_cloned_reader, req);
    while (req->valid) {
      local_cache->get(local_cache, req);
      result[idx].n_warmup_req += 1;
      read_one_req(warmup_cloned_reader, req);
    }
    close_reader(warmup_cloned_reader);
    INFO("cache %s (size %" PRIu64
         ") finishes warm up using warmup reader "
         "with %" PRIu64 " requests\n",
         local_cache->cache_name, local_cache->cache_size, result[idx].n_warmup_req);
  }

  read_one_req(cloned_reader, req);
  int64_t start_ts = (int64_t)req->clock_time;

  /* using warmup_frac or warmup_sec of requests from reader to warm up */
  if (params->n_warmup_req > 0 || params->warmup_sec > 0) {
    uint64_t n_warmup = 0;
    while (req->valid && (n_warmup < params->n_warmup_req || req->clock_time - start_ts < params->warmup_sec)) {
      req->clock_time -= start_ts;
      local_cache->get(local_cache, req);
      n_warmup += 1;
      read_one_req(cloned_reader, req);
    }
    result[idx].n_warmup_req += n_warmup;
    INFO("cache %s (size %" PRIu64
         ") finishes warm up using "
         "with %" PRIu64 " requests, %.2lf hour trace time\n",
         local_cache->cache_name, local_cache->cache_size, n_warmup, (double)(req->clock_time - start_ts) / 3600.0);
  }

  while (req->valid) {
    result[idx].n_req++;
    result[idx].n_req_byte += req->obj_size;

    req->clock_time -= start_ts;
    if (local_cache->get(local_cache, req) == false) {
      result[idx].n_miss++;
      result[idx].n_miss_byte += req->obj_size;
    }
    read_one_req(cloned_reader, req);
  }

/* disabled due to ARC and LeCaR use ghost entries in the hash table */
#if defined(SUPPORT_TTL) && defined(ENABLE_SCAN)
  /* get expiration information */
  if (local_cache->hashtable->n_obj != 0) {
    cache_stat_t temp_stat;
    memset(&temp_stat, 0, sizeof(cache_stat_t));
    temp_stat.curr_rtime = req->clock_time;
    get_cache_state(local_cache, &temp_stat);

    if (local_cache->occupied_size != temp_stat.occupied_size) {
      WARN(
          "occupied_size not match, %ld vs %ld, maybe the "
          "cache uses a ghost list, in which case, the expired "
          "object count may not be accurate",
          local_cache->occupied_size, temp_stat.occupied_size);
    }
    result[idx].expired_obj_cnt = temp_stat.expired_obj_cnt;
    result[idx].expired_bytes = temp_stat.expired_bytes;
  }
#endif

  result[idx].curr_rtime = req->clock_time;
  result[idx].n_obj = local_cache->n_obj;
  result[idx].occupied_byte = local_cache->occupied_byte;
  generate_cache_name(local_cache, result[idx].cache_name);

  // report progress
  g_mutex_lock(&(params->mtx));
  (*(params->progress))++;
  g_mutex_unlock(&(params->mtx));

  // clean up
  if (params->free_cache_when_finish) {
    local_cache->cache_free(local_cache);
  }
  free_request(req);
  close_reader(cloned_reader);
}

cache_stat_t *simulate_at_multi_sizes_with_step_size(reader_t *const reader, const cache_t *cache, uint64_t step_size,
                                                     reader_t *warmup_reader, double warmup_frac, int warmup_sec,
                                                     int num_of_threads, bool use_random_seed) {
  int num_of_sizes = (int)ceil((double)cache->cache_size / (double)step_size);
  get_num_of_req(reader);
  uint64_t *cache_sizes = my_malloc_n(uint64_t, num_of_sizes);
  for (int i = 0; i < num_of_sizes; i++) {
    cache_sizes[i] = step_size * (i + 1);
  }

  cache_stat_t *res = simulate_at_multi_sizes(reader, cache, num_of_sizes, cache_sizes, warmup_reader, warmup_frac,
                                              warmup_sec, num_of_threads, use_random_seed);
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
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache, int num_of_sizes,
                                      const uint64_t *cache_sizes, reader_t *warmup_reader, double warmup_frac,
                                      int warmup_sec, int num_of_threads, bool use_random_seed) {
  int progress = 0;

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_sizes);
  memset(result, 0, sizeof(cache_stat_t) * num_of_sizes);

  // build parameters and send to thread pool
  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->reader = reader;
  params->readers = NULL;
  params->warmup_reader = warmup_reader;
  params->warmup_sec = warmup_sec;
  params->n_caches = num_of_sizes;
  params->n_warmup_req = (uint64_t)((double)get_num_of_req(reader) * warmup_frac);
  params->result = result;
  params->free_cache_when_finish = true;
  params->progress = &progress;
  params->use_random_seed = use_random_seed;
  g_mutex_init(&(params->mtx));

  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new((GFunc)_simulate, (gpointer)params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  // start computation
  params->caches = my_malloc_n(cache_t *, num_of_sizes);
  for (int i = 1; i < num_of_sizes + 1; i++) {
    params->caches[i - 1] = create_cache_with_new_size(cache, cache_sizes[i - 1]);
    result[i - 1].cache_size = cache_sizes[i - 1];
    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(cache_sizes[0], start_cache_size);
  convert_size_to_str(cache_sizes[num_of_sizes - 1], end_cache_size);

  INFO(
      "%s starts computation %s, num_warmup_req %lld, start cache size %s, "
      "end cache size %s, %d sizes, %d threads, please wait\n",
      __func__, cache->cache_name, (long long)(params->n_warmup_req), start_cache_size, end_cache_size, num_of_sizes,
      num_of_threads);

  // wait for all simulations to finish
  while (progress < (uint64_t)num_of_sizes - 1) {
    print_progress((double)progress / (double)(num_of_sizes - 1) * 100);
  }

  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  my_free(sizeof(cache_t *) * num_of_sizes, params->caches);
  my_free(sizeof(sim_mt_params_t), params);

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
cache_stat_t *simulate_with_multi_caches(reader_t *reader, cache_t *caches[], int num_of_caches,
                                         reader_t *warmup_reader, double warmup_frac, int warmup_sec,
                                         int num_of_threads, bool free_cache_when_finish, bool use_random_seed) {
  assert(num_of_caches > 0);
  int i, progress = 0;

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_caches);
  memset(result, 0, sizeof(cache_stat_t) * num_of_caches);

  // build parameters and send to thread pool
  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->reader = reader;
  params->readers = NULL;
  params->caches = caches;
  params->warmup_reader = warmup_reader;
  params->warmup_sec = warmup_sec;
  params->use_random_seed = use_random_seed;
  if (warmup_frac > 1e-6) {
    params->n_warmup_req = (uint64_t)((double)get_num_of_req(reader) * warmup_frac);
  } else {
    params->n_warmup_req = 0;
  }
  params->result = result;
  params->free_cache_when_finish = free_cache_when_finish;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));

  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new((GFunc)_simulate, (gpointer)params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  // start computation
  for (i = 1; i < num_of_caches + 1; i++) {
    result[i - 1].cache_size = caches[i - 1]->cache_size;

    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(result[0].cache_size, start_cache_size);
  convert_size_to_str(result[num_of_caches - 1].cache_size, end_cache_size);

  INFO(
      "%s starts computation, num_warmup_req %lld, start cache %s size %s, "
      "end cache %s size %s, %d caches, %d threads, please wait\n",
      __func__, (long long)(params->n_warmup_req), caches[0]->cache_name, start_cache_size,
      caches[num_of_caches - 1]->cache_name, end_cache_size, num_of_caches, num_of_threads);

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

cache_stat_t *simulate_with_multi_caches_scaling(reader_t **readers, cache_t *caches[], int num_of_caches,
                                                 reader_t *warmup_reader, double warmup_frac, int warmup_sec,
                                                 int num_of_threads, bool free_cache_when_finish) {
  int progress = 0;

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_caches);
  memset(result, 0, sizeof(cache_stat_t) * num_of_caches);

  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->readers = readers;  // use multi-readers for scaling
  params->reader = NULL;      // not used in scaling mode
  params->caches = caches;
  params->warmup_reader = warmup_reader;
  params->warmup_sec = warmup_sec;
  params->use_random_seed = false;  // or set as desired
  if (warmup_frac > 1e-6) {
    params->n_warmup_req = (uint64_t)((double)get_num_of_req(readers[0]) * warmup_frac);
  } else {
    params->n_warmup_req = 0;
  }
  params->result = result;
  params->free_cache_when_finish = free_cache_when_finish;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));

  GThreadPool *gthread_pool = g_thread_pool_new((GFunc)_simulate, (gpointer)params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  for (int i = 1; i < num_of_caches + 1; i++) {
    result[i - 1].cache_size = caches[i - 1]->cache_size;
    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(result[0].cache_size, start_cache_size);
  convert_size_to_str(result[num_of_caches - 1].cache_size, end_cache_size);

  INFO(
      "simulate_with_multi_caches_scaling starts computation, num_warmup_req %lld, start cache size %s, end cache size "
      "%s, %d caches, %d threads, please wait\n",
      (long long)params->n_warmup_req, start_cache_size, end_cache_size, num_of_caches, num_of_threads);

  while (progress < (uint64_t)num_of_caches - 1) {
    print_progress((double)progress / (double)(num_of_caches - 1) * 100);
  }

  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  my_free(sizeof(sim_mt_params_t), params);
  for (int i=0; i<num_of_caches; i++) {
    result[i].sampler_ratio = readers[i]->sampler->sampling_ratio;
  }
  return result;
}

#ifdef __cplusplus
}
#endif

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
#include "../cache/cacheUtils.h"
#include "../include/libCacheSim/plugin.h"
#include "../utils/include/myprint.h"
#include "../utils/include/mystr.h"


typedef struct simulator_multithreading_params {
  reader_t *reader;
  cache_t *cache;
  uint64_t n_warmup_req; /* num of requests used for warming up cache if using
                            requests from reader */
  reader_t *warmup_reader;
  cache_stat_t *result;
  GMutex mtx; /* prevent simultaneous write to progress */
  gint *progress;
  gpointer other_data;
} sim_mt_params_t;

static void get_mrc_thread(gpointer data, gpointer user_data) {
  sim_mt_params_t *params = (sim_mt_params_t *) user_data;
  int idx = GPOINTER_TO_UINT(data) - 1;

  cache_stat_t *result = params->result;
  reader_t *cloned_reader = clone_reader(params->reader);
  request_t *req = new_request();
  cache_t *local_cache =
      create_cache_with_new_size(params->cache, result[idx].cache_size);

  /* warm up using warmup_reader */
  if (params->warmup_reader) {
    reader_t *warmup_cloned_reader = clone_reader(params->warmup_reader);
    read_one_req(warmup_cloned_reader, req);
    while (req->valid) {
      cache_ck_res_e ck = local_cache->get(local_cache, req);
      local_cache->stat.n_warmup_req += 1;
#if defined(SUPPORT_ADMISSION) && SUPPORT_ADMISSION == 1
      if (local_cache->admit != NULL && ck == cache_ck_miss)
        local_cache->admit(local_cache, req);
#endif
      read_one_req(warmup_cloned_reader, req);
    }
    close_reader(warmup_cloned_reader);
    INFO("cache %s (size %"PRIu64 ") finishes warm up using warmup reader "
            "with %"PRIu64 "requests\n", local_cache->cache_name,
            local_cache->cache_size, local_cache->stat.n_warmup_req);
  }

  /* using warmup_frac of requests from reader to warm up */
  read_one_req(cloned_reader, req);
  if (params->n_warmup_req > 0) {
    uint64_t n_warmup = 0;
    while (req->valid && n_warmup < params->n_warmup_req) {
      cache_ck_res_e ck = local_cache->get(local_cache, req);
#if defined(SUPPORT_ADMISSION) && SUPPORT_ADMISSION == 1
      if (local_cache->admit != NULL && ck == cache_ck_miss)
        local_cache->admit(local_cache, req);
#endif
      n_warmup += 1;
      read_one_req(cloned_reader, req);
    }
    local_cache->stat.n_warmup_req += n_warmup;
    INFO("cache %s (size %"PRIu64 ") finishes warm up using the same reader "
            "with %"PRIu64 " requests\n", local_cache->cache_name,
            local_cache->cache_size, n_warmup);
  }

  int64_t start_ts = (int64_t) req->real_time;
  req->real_time -= start_ts;

  while (req->valid) {
    local_cache->stat.n_req++;
    local_cache->stat.n_req_byte += req->obj_size;

    if (local_cache->check(local_cache, req, true) != cache_ck_hit) {
      local_cache->stat.n_miss++;
      local_cache->stat.n_miss_byte += req->obj_size;

      bool admit = true;

#if defined(SUPPORT_ADMISSION) && SUPPORT_ADMISSION == 1
      if (local_cache->admit != NULL && !!local_cache->admit(local_cache, req)) {
          admit = false;
      }
#endif

      if (admit) {
        if (req->obj_size > local_cache->cache_size) {
          WARNING("object %"PRIu64 ": obj size %"PRIu32 " larger than cache size %"PRIu64 "\n",
                  req->obj_id, req->obj_size, local_cache->cache_size);
        }

        while (local_cache->occupied_size + req->obj_size +
                local_cache->per_obj_overhead > local_cache->cache_size)
          local_cache->evict(local_cache, req, NULL);

        local_cache->insert(local_cache, req);
      }
    }
    read_one_req(cloned_reader, req);
    req->real_time -= start_ts;
  }

  /* get expiration information */
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (local_cache->default_ttl != 0) {
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
  local_cache->cache_free(local_cache);
}

cache_stat_t *get_miss_ratio_curve_with_step_size(reader_t *const reader,
                                                  const cache_t *cache,
                                                  uint64_t step_size,
                                                  reader_t *warmup_reader,
                                                  double warmup_frac,
                                                  int num_of_threads) {

  int num_of_sizes = (int) ceil((double) cache->cache_size / (double) step_size);
  get_num_of_req(reader);
  uint64_t *cache_sizes = my_malloc_n(uint64_t, num_of_sizes);
  for (int i = 0; i < num_of_sizes; i++) {
    cache_sizes[i] = step_size * (i + 1);
  }

  cache_stat_t *res =
      get_miss_ratio_curve(reader, cache, num_of_sizes, cache_sizes,
                           warmup_reader, warmup_frac, num_of_threads);
  my_free(sizeof(uint64_t) * num_of_sizes, cache_sizes);
  return res;
}

cache_stat_t *
get_miss_ratio_curve(reader_t *reader, const cache_t *cache,
                     int num_of_sizes, const uint64_t *cache_sizes,
                     reader_t *warmup_reader, double warmup_frac,
                     int num_of_threads) {

  int i, progress = 0;
  get_num_of_req(reader);

  cache_stat_t *result = my_malloc_n(cache_stat_t, num_of_sizes);

  // build parameters and send to thread pool
  sim_mt_params_t *params = my_malloc(sim_mt_params_t);
  params->reader = reader;
  params->warmup_reader = warmup_reader;
  params->cache = (cache_t *) cache;
  params->n_warmup_req = (uint64_t)((double) get_num_of_req(reader) * warmup_frac);
  params->result = result;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));

  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new(
      (GFunc) get_mrc_thread, (gpointer) params, num_of_threads, TRUE, NULL);
  ASSERT_NOT_NULL(gthread_pool, "cannot create thread pool in simulator\n");

  // start computation
  for (i = 1; i < num_of_sizes + 1; i++) {
    result[i-1].cache_size = cache_sizes[i-1];
    ASSERT_TRUE(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }
  sleep(2);

  char start_cache_size[64], end_cache_size[64];
  convert_size_to_str(cache_sizes[0], start_cache_size);
  convert_size_to_str(cache_sizes[num_of_sizes - 1], end_cache_size);

  INFO("%s starts computation %s, num_warmup_req %lld, start cache size %s, "
       "end cache size %s, %d sizes, %d threads, please wait\n",
       __func__, cache->cache_name, (long long) (params->n_warmup_req),
       start_cache_size, end_cache_size, num_of_sizes, num_of_threads);

  // wait for all simulations to finish
  while (progress < (uint64_t) num_of_sizes - 1) {
    print_progress((double) progress / (double) (num_of_sizes - 1) * 100);
  }

  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  my_free(sizeof(sim_mt_params_t), params);
  // user is responsible to free the result
  return result;
}

#ifdef __cplusplus
}
#endif

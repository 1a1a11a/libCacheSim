//
//  profiler.c
//  libMimircache
//
//  Created by Juncheng on 11/20/19.
//  Copyright © 2016-2019 Juncheng. All rights reserved.
//




#ifdef __cplusplus
extern "C"
{
#endif


#include "../include/mimircache/profiler.h"
#include "../include/mimircache/plugin.h"
#include "include/profilerInternal.h"
#include "../utils/include/utilsInternal.h"


static void _get_mrc_thread(gpointer data, gpointer user_data) {
  prof_mt_params_t *params = (prof_mt_params_t *) user_data;
  int idx = GPOINTER_TO_UINT(data)-1;

//  cache_t *cache = params->cache->core->cacheit(bin_size * idx,
//                                                   params->cache->core->obj_id_type,
//                                                   params->cache->core->block_size,
//                                                   params->cache->core->cacheit_params);

  profiler_res_t *result = params->result;
  reader_t *cloned_reader = clone_reader(params->reader);
  request_t *req = new_request(params->cache->core->obj_id_type);
  cache_t *local_cache = create_cache(params->cache->core->cache_name, result[idx].cache_size,
                                      params->cache->core->obj_id_type, params->cache->core->cache_init_params);


  guint64 req_cnt = 0, miss_cnt = 0;
  guint64 req_byte = 0, miss_byte = 0;
  gboolean (*add)(cache_t *, request_t *);
  add = local_cache->core->get;

  read_one_req(cloned_reader, req);
  if (params->n_warmup_req > 0){
    guint64 n_warmup = 0;
    while (req->valid && n_warmup < params->n_warmup_req) {
      read_one_req(cloned_reader, req);
      n_warmup += 1;
    }
  }

  while (req->valid) {
    req_cnt++;
    req_byte += req->size;
    if (!add(local_cache, req)) {
      miss_cnt++;
      miss_byte += req->size;
    }
    read_one_req(cloned_reader, req);
  }

  result[idx].miss_byte = miss_byte;
  result[idx].miss_cnt = miss_cnt;
  result[idx].req_cnt = req_cnt;
  result[idx].req_byte = req_byte;
  result[idx].obj_miss_ratio = (double) miss_cnt / (double) req_cnt;
  result[idx].byte_miss_ratio = (double) miss_byte / (double) req_byte;

  // report progress
  g_mutex_lock(&(params->mtx));
  (*(params->progress))++;
  g_mutex_unlock(&(params->mtx));

  // clean up
  free_request(req);
  close_cloned_reader(cloned_reader);
  local_cache->core->cache_free(local_cache);
}

profiler_res_t * get_miss_ratio_curve_with_step_size(reader_t *const reader, const cache_t *const cache,
                                    const guint64 step_size, const double warmup_perc, const gint num_of_threads) {

  int num_of_sizes = (int) ceil((double) cache->core->size / step_size) ;
  get_num_of_req(reader);
  guint64 *cache_sizes = g_new0(guint64, num_of_sizes);
  for (int i = 0; i < num_of_sizes; i++) {
    cache_sizes[i] = step_size * (i+1);
  }

  profiler_res_t *res = get_miss_ratio_curve(reader, cache, num_of_sizes, cache_sizes, warmup_perc, num_of_threads);
  g_free(cache_sizes);
  return res;
}


profiler_res_t *get_miss_ratio_curve(reader_t *const reader, const cache_t *const cache,
                                     const gint num_of_sizes, const guint64 *const cache_sizes,
                                     const double warmup_perc, const gint num_of_threads) {

  guint64 i, progress = 0;
  get_num_of_req(reader);

  profiler_res_t *result = g_new0(profiler_res_t, num_of_sizes);
  for (i = 0; i < num_of_sizes; i++) {
    result[i].cache_size = cache_sizes[i];
  }

  // build parameters and send to thread pool
  prof_mt_params_t *params = g_new0(prof_mt_params_t, 1);
  params->reader = reader;
  params->cache = (cache_t*) cache;
  params->n_warmup_req = (guint64)(reader->base->n_total_req * warmup_perc);
  params->result = result;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));


  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new((GFunc) _get_mrc_thread,
                                                (gpointer) params, num_of_threads, TRUE, NULL);
  check_null(gthread_pool, "cannot create thread pool in profiler::evaluate\n");

  // start computation
  for (i = 1; i < num_of_sizes+1; i++) {
    check_false(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }
  sleep(2);

  char start_cache_size[8], end_cache_size[8];
  convert_size_to_str(cache_sizes[0], start_cache_size);
  convert_size_to_str(cache_sizes[num_of_sizes-1], end_cache_size);

  INFO("%s starts computation, num_warmup_req %lld, start cache size %s, end cache size %s, %d sizes, please wait\n",
      __func__, (long long) (params->n_warmup_req), start_cache_size, end_cache_size, num_of_sizes);

  // wait for all simulations to finish
  while (progress < (guint64) num_of_sizes - 1) {
    print_progress((double) progress / (double) (num_of_sizes - 1) * 100);
  }

  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  g_free(params);
  // user is responsible for g_free result
  return result;
}


#ifdef __cplusplus
}
#endif

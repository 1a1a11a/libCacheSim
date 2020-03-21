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
  int idx = GPOINTER_TO_UINT(data);

//  cache_t *cache = params->cache->core->cache_init(bin_size * idx,
//                                                   params->cache->core->obj_id_type,
//                                                   params->cache->core->block_size,
//                                                   params->cache->core->cache_init_params);

  cache_t *local_cache = create_cache(params->cache->core->cache_name, params->bin_size * idx,
                                      params->cache->core->obj_id_type, params->cache->core->cache_init_params);
  profiler_res_t *result = params->result;
  reader_t *cloned_reader = clone_reader(params->reader);
  request_t *req = new_request(params->cache->core->obj_id_type);


  guint64 req_cnt = 0, miss_cnt = 0;
  guint64 req_byte = 0, miss_byte = 0;
  gboolean (*add)(cache_t *, request_t *);
  add = local_cache->core->add;

  read_one_req(cloned_reader, req);

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
  local_cache->core->destroy_unique(local_cache);
}

profiler_res_t *get_miss_ratio_curve(reader_t *reader_in, cache_t *cache_in, int num_of_threads, guint64 bin_size) {

  guint64 i, progress = 0;
  guint64 num_of_caches = (guint64) ceil((double) cache_in->core->size / bin_size) + 1;

  get_num_of_req(reader_in);

  // allocate memory for result 
//  profiler_res_t **result = g_new(profiler_res_t*, num_of_caches);
//  for (i = 0; i < num_of_caches; i++) {
//    result[i] = g_new0(profiler_res_t, 1);
//    // create caches of varying sizes
//    result[i]->cache_size = bin_size * i;
//  }

  profiler_res_t *result = g_new0(profiler_res_t, num_of_caches);
  for (i = 0; i < num_of_caches; i++) {
    result[i].cache_size = bin_size * i;
  }

  // size 0 always has miss ratio 1
  result[0].obj_miss_ratio = 1;
  result[0].byte_miss_ratio = 1;

  // build parameters and send to thread pool
  prof_mt_params_t *params = g_new0(prof_mt_params_t, 1);
  params->reader = reader_in;
  params->cache = cache_in;
  params->result = result;
  params->bin_size = (guint) bin_size;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));


  // build the thread pool
  GThreadPool *gthread_pool = g_thread_pool_new((GFunc) _get_mrc_thread,
                                                (gpointer) params, num_of_threads, TRUE, NULL);
  check_null(gthread_pool, "cannot create thread pool in profiler::evaluate\n");

  // start computation
  for (i = 1; i < num_of_caches; i++) {
    check_false(g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL),
                "cannot push data into thread_pool in get_miss_ratio\n");
  }

  // wait for all simulations to finish
  sleep(2);
  INFO("%s starts computation, please wait\n", __func__);
  while (progress < (guint64) num_of_caches - 1) {
    print_progress((double) progress / (double) (num_of_caches - 1) * 100);
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

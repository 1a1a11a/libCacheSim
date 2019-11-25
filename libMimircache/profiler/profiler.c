//
//  profiler.c
//  mimircache
//
//  Created by Juncheng on 11/20/19.
//  Copyright © 2016-2019 Juncheng. All rights reserved.
//



#include "../include/mimircache/profiler.h"
#include "../include/mimircache/plugin.h"


#ifdef __reqlusplus
extern "C"
{
#endif


static void profiler_thread(gpointer data, gpointer user_data) {
  prof_mt_params_t *params = (prof_mt_params_t *) user_data;

  int idx = GPOINTER_TO_UINT(data);

//  cache_t *cache = params->cache->core->cache_init(bin_size * idx,
//                                                   params->cache->core->obj_id_type,
//                                                   params->cache->core->block_size,
//                                                   params->cache->core->cache_init_params);

  cache_t *cache = create_cache(params->cache->core->cache_name, params->bin_size*idx,
    params->cache->core->obj_id_type, params->cache->core->cache_init_params);
  profiler_res_t **result = params->result;
  reader_t *reader_thread = clone_reader(params->reader);
  request_t *req = new_request(params->cache->core->obj_id_type);


  guint64 req_cnt = 0, miss_cnt = 0;
  guint64 req_byte = 0, miss_byte = 0;
  gboolean (*add)(struct cache *, request_t *req);
  add = cache->core->add;

  read_one_req(reader_thread, req);

  while (req->valid) {
    req_cnt ++;
    req_byte += req->size;
    if (!add(cache, req)){
      miss_cnt++;
      miss_byte += req->size;
    }
    read_one_req(reader_thread, req);
  }

  result[idx]->miss_byte = miss_byte;
  result[idx]->miss_cnt = miss_cnt;
  result[idx]->req_cnt = req_cnt;
  result[idx]->req_byte = req_byte;
  result[idx]->obj_miss_ratio = (double) miss_cnt / (double) req_cnt;
  result[idx]->byte_miss_ratio = (double) miss_byte / (double) req_byte;

  // report progress
  g_mutex_lock(&(params->mtx));
  (*(params->progress))++;
  g_mutex_unlock(&(params->mtx));

  // clean up
  free_request(req);
  close_reader_unique(reader_thread);
  cache->core->destroy_unique(cache);
}

profiler_res_t **run_trace(reader_t *reader_in,
                           cache_t *cache_in,
                           int num_of_threads,
                           guint64 bin_size) {

  guint64 i, progress = 0;
  guint64 num_of_caches = (guint64) ceil((double) cache_in->core->size / bin_size) + 1;

  if (reader_in->base->n_total_req == -1)
    get_num_of_req(reader_in);

  // allocate memory for result 
  profiler_res_t **result = g_new(profiler_res_t*, num_of_caches);
  for (i = 0; i < num_of_caches; i++) {
    result[i] = g_new0(profiler_res_t, 1);
    // create caches of varying sizes
    result[i]->cache_size = bin_size * i;
  }

  // size 0 always has miss ratio 1
  result[0]->obj_miss_ratio = 1;
  result[0]->byte_miss_ratio = 1;

  // build parameters and send to thread pool
  prof_mt_params_t *params = g_new0(prof_mt_params_t, 1);
  params->reader = reader_in;
  params->cache = cache_in;
  params->result = result;
  params->bin_size = (guint) bin_size;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));


  // build the thread pool
  GThreadPool *gthread_pool;
  gthread_pool = g_thread_pool_new((GFunc) profiler_thread,
                                   (gpointer) params, num_of_threads, TRUE, NULL);
  if (gthread_pool == NULL) ERROR("cannot create thread pool in profiler::run_trace\n");


  // start computation
  for (i = 1; i < num_of_caches; i++) {
    if (g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i), NULL) == FALSE)
      ERROR("cannot push data into thread_pool in get_miss_ratio\n");
  }

  // wait for all simulations to finish
  sleep(2);
  INFO("%s starts computation, please wait\n", __func__);
  sleep(20);
  while (progress < (guint64) num_of_caches - 1) {
    fprintf(stderr, "%.2f%%\n", ((double) progress) / (double) (num_of_caches - 1) * 100);
    sleep(20);
    fprintf(stderr, "\033[A\033[2K\r");
  }


  // clean up
  g_thread_pool_free(gthread_pool, FALSE, TRUE);
  g_mutex_clear(&(params->mtx));
  g_free(params);
  // user is responsible for g_free result
  return result;
}


void traverse_trace(reader_t *reader, cache_t *cache) {

  // req struct creation and initialization
  request_t *req = new_request(cache->core->obj_id_type);
  gboolean (*add)(cache_t*, request_t *);
  add = cache->core->add;

  read_one_req(reader, req);
  while (req->valid) {
    add(cache, req);
    read_one_req(reader, req);
  }

  // clean up
  free_request(req);
  reset_reader(reader);
}


#ifdef __reqlusplus
}
#endif

//
// measured the performance of cache eviction algorithm
//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/cacheAlgoPerf.h"
#include "../include/mimircache/request.h"


gint64 measure_qps_write(cache_t *cache, double expected_miss_ratio) {
  srand(time(NULL));
  request_t *req = new_request(OBJ_ID_NUM);
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_ptr = (gpointer) 0L;
  req->obj_size = 1024;

  struct timeval t0;
  gettimeofday(&t0, 0);
  guint64 n_hit = 0, n_req = 0;

  while (time_since(t0) < 20) {
    for (int i = 0; i < 20 * 1000; i++) {
      if (cache->core->get(cache, req))
        n_hit += 1;
      n_req += 1;
      req->obj_id_ptr = (gpointer )((guint64) req->obj_id_ptr + 1);
    }
  }
  double elapsed_time = time_since(t0);
  printf("write %s %llu req, %llu hit in %.2lf sec (%.2lf KQPS)\n", cache->core->cache_name, (unsigned long long) n_req,
         (unsigned long long) n_hit,
         elapsed_time, (double) n_req / elapsed_time / 1000);
  return (double) n_req / elapsed_time;
}

gint64 measure_qps_read(cache_t *cache, double expected_miss_ratio) {
  srand(time(NULL));
  request_t *req = new_request(OBJ_ID_NUM);
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_ptr = (gpointer) (guint64) rand();
  req->obj_size = 1024;

  struct timeval t0;
  guint64 n_hit = 0, n_req = 0;

  for (int i = 0; i < 200 * 1000; i++) {
    cache->core->get(cache, req);
    req->obj_id_ptr = (gpointer)((guint64) i + 1);
  }

  gettimeofday(&t0, 0);
  while (time_since(t0) < 20) {
    gint32 start = rand()%(180*1000);
    for (int i = 0; i < 2 * 1000; i++) {
      if (cache->core->get(cache, req))
        n_hit += 1;
      n_req += 1;
      req->obj_id_ptr = (gpointer)(guint64)(start + 1);
    }
  }
  double elapsed_time = time_since(t0);
  printf("read %s %llu req, %llu hit in %.2lf sec (%.2lf KQPS)\n", cache->core->cache_name, (unsigned long long) n_req,
         (unsigned long long) n_hit,
         elapsed_time, (double) n_req / elapsed_time / 1000);
  return (double) n_req / elapsed_time;
}
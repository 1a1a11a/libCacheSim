//
// measured the performance of cache eviction algorithm
//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/cacheAlgoPerf.h"
#include "../include/mimircache/request.h"


gint64 measure_qps(cache_t* cache, double expected_miss_ratio){
  srand(time(NULL));
  request_t * req = new_request(OBJ_ID_NUM);
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_ptr = (gpointer) (guint64) rand();
  req->obj_size = 1024;

  struct timeval t0;
  gettimeofday(&t0, 0);
  guint64 n_hit = 0, n_req = 0;

  while (time_since(t0) < 20){
    for (int j=0; j<100; j++){
      for (int i=0; i<1000; i++){
        if (cache->core->get(cache, req))
          n_hit += 1;
        n_req += 1;
        req->obj_id_ptr += ((guint64) req->obj_id_ptr + 1);
      }
      req->obj_id_ptr = (gpointer)(guint64) rand();
    }
  }
  double elapsed_time = time_since(t0);
  printf("%s %llu req, %llu hit in %.2lf sec (%.2lf KQPS)\n", cache->core->cache_name, (unsigned long long) n_req, (unsigned long long) n_hit,
      elapsed_time, (double) n_req/elapsed_time/1000);
  return (double) n_req/elapsed_time;
}
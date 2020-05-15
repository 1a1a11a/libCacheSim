//
// measured the performance of cache eviction algorithm
//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/params.h"
#include "include/cacheAlgoPerf.h"
#include "../include/mimircache/request.h"
#include "include/resourceMeasure.h"
#include "../utils/include/mathUtils.h"


gint64 measure_qps_write(cache_t *cache) {
  srand(time(NULL));
  request_t *req = new_request(OBJ_ID_NUM);
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_ptr = (gpointer) 0L;
  req->obj_size = OBJ_SIZE;

  struct timeval t0;
  gettimeofday(&t0, 0);
  guint64 n_hit = 0, n_req = 0;

  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);
//  print_resource_usage();

  guint64 n_obj = 0;
  while (time_since(t0) < MAX_RUNTIME && n_obj < cache->core.size/OBJ_SIZE ) {
    for (int i = 0; i < 20 * 1000; i++) {
      if (cache->core.get(cache, req))
        n_hit += 1;
      n_req += 1;
      req->obj_id_ptr += MEM_ALIGN_STRIPE;
      n_obj ++;
    }
  }
  double elapsed_time = time_since(t0);

  getrusage(RUSAGE_SELF, &r_usage_after);
//  print_resource_usage();
  print_rusage_diff(r_usage_before, r_usage_after);



  printf("write %s %llu req, %llu hit in %.2lf sec (%.2lf KQPS)\n", cache->core.cache_name, (unsigned long long) n_req,
         (unsigned long long) n_hit, elapsed_time, (double) n_req / elapsed_time / 1000);
  printf("**********************************************************\n\n");
  return (double) n_req / elapsed_time;
}

gint64 measure_qps_read(cache_t *cache) {
  srand(time(NULL));
  request_t *req = new_request(OBJ_ID_NUM);
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_ptr = (gpointer) 0L;
  req->obj_size = OBJ_SIZE;

  struct timeval t0;
  guint64 n_hit = 0, n_req = 0;

  for (int i = 0; i < cache->core.size/OBJ_SIZE; i++) {
    cache->core.get(cache, req);
    req->obj_id_ptr += MEM_ALIGN_STRIPE;
  }

  req->obj_id_ptr = (gpointer) 0L;
  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);

  gettimeofday(&t0, 0);
  while (time_since(t0) < MAX_RUNTIME) {
    gint64 start = next_rand()%(cache->core.size/OBJ_SIZE - 2*1000) * MEM_ALIGN_STRIPE;
    req->obj_id_ptr = (gpointer) start;
    for (int i = 0; i < 2 * 1000; i++) {
      if (cache->core.get(cache, req))
        n_hit += 1;
      n_req += 1;
      req->obj_id_ptr += MEM_ALIGN_STRIPE;
    }
  }
  double elapsed_time = time_since(t0);

  getrusage(RUSAGE_SELF, &r_usage_after);
//  print_resource_usage();
  print_rusage_diff(r_usage_before, r_usage_after);


  printf("read %s %llu req, %llu hit in %.2lf sec (%.2lf KQPS)\n", cache->core.cache_name, (unsigned long long) n_req,
         (unsigned long long) n_hit,
         elapsed_time, (double) n_req / elapsed_time / 1000);
  printf("**********************************************************\n\n");
  return (double) n_req / elapsed_time;
}
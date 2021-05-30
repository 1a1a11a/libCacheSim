//
// measured the performance of cache eviction algorithm
//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/cacheAlgoPerf.h"
#include "../include/libCacheSim/request.h"
#include "include/params.h"
#include "include/resourceMeasure.h"
#include "../utils/include/mymath.h"

void prepare_cache(cache_t *cache, request_t *req) {
  req->valid = TRUE;
  req->real_time = 1;
  req->obj_id_int = 0;
  req->obj_size = OBJ_SIZE;

  for (int i = 0; i < cache->cache_size/OBJ_SIZE; i++) {
    cache->get(cache, req);
    req->obj_id_int += 1;
  }
  printf("done warming up cache\n");
}

int measure_qps_write(cache_t *cache) {
  request_t *req = new_request();
  prepare_cache(cache, req);

  unsigned long n_hit = 0, n_req = 0;
  struct timeval t0;
  gettimeofday(&t0, 0);

  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);
//  print_resource_usage();

  while (time_since(t0) < MAX_RUNTIME) {
    for (int i = 0; i < 20 * 1000; i++) {
      if (cache->get(cache, req) == cache_ck_hit)
        n_hit += 1;
      n_req += 1;
      req->obj_id_int += 1;
    }
  }
  double elapsed_time = time_since(t0);

  getrusage(RUSAGE_SELF, &r_usage_after);
  print_rusage_diff(r_usage_before, r_usage_after);


  printf("write %s %lu req/obj, %lu hit in %.2lf sec (%.2lf KQPS)\n",
         cache->cache_name, n_req, n_hit, elapsed_time, (double) n_req / elapsed_time / 1000);
  printf("**********************************************************\n\n");
  return (int) ((double) n_req / elapsed_time);
}

int measure_qps_read(cache_t *cache) {
  request_t *req = new_request();
  prepare_cache(cache, req);

  struct timeval t0;
  unsigned long n_hit = 0, n_req = 0;

  req->obj_id_int = 0;
  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);

  gettimeofday(&t0, 0);
  while (time_since(t0) < MAX_RUNTIME) {
    uint64_t start = next_rand()%(cache->cache_size/OBJ_SIZE - 2*1000);
    req->obj_id_int = start;
    for (int i = 0; i < 2 * 1000; i++) {
      if (cache->get(cache, req) == cache_ck_hit)
        n_hit += 1;
      n_req += 1;
      req->obj_id_int += 1;
    }
  }
  double elapsed_time = time_since(t0);

  getrusage(RUSAGE_SELF, &r_usage_after);
  print_rusage_diff(r_usage_before, r_usage_after);


  printf("read %s %lu req, %lu hit in %.2lf sec (%.2lf KQPS)\n",
         cache->cache_name, n_req, n_hit,
         elapsed_time, (double) n_req / elapsed_time / 1000);
  printf("**********************************************************\n\n");
  return (int) ((double) n_req / elapsed_time);
}


int measure_qps_withtrace(cache_t* cache, reader_t* reader){
  request_t *req = new_request();
  prepare_cache(cache, req);

  struct timeval t0;
  unsigned long n_hit = 0, n_req = 0;

  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);
  gettimeofday(&t0, 0);

  read_one_req(reader, req);
  while (req->valid) {
      if (cache->get(cache, req) == cache_ck_hit)
        n_hit += 1;
      n_req += 1;
    read_one_req(reader, req);
  }

  double elapsed_time = time_since(t0);
  getrusage(RUSAGE_SELF, &r_usage_after);
  print_rusage_diff(r_usage_before, r_usage_after);

  printf("read %s %lu req, %lu hit in %.2lf sec (%.2lf KQPS)\n",
         cache->cache_name, n_req, n_hit,
         elapsed_time, (double) n_req / elapsed_time / 1000);
  printf("**********************************************************\n\n");
  return (int) ((double) n_req / elapsed_time);
}
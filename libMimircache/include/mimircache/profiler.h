//
//  generalProfiler.h
//  generalProfiler
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef profiler_h
#define profiler_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <string.h>
#include "reader.h"
#include "cache.h"
#include "const.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct {
  guint64 req_cnt;
  guint64 req_byte;
  guint64 miss_cnt;
  guint64 miss_byte;
  double obj_miss_ratio;
  double byte_miss_ratio;
  gint64 cache_size;
  gpointer other_data;
} profiler_res_t;


typedef enum {
  P_MISS_RATIO = 0,
  P_EVICTION_AGE,
  P_HIT_RESULT,
  P_EVICTION,
} profiler_t;

typedef struct profiler_multithreading_params {
  reader_t *reader;
  struct cache *cache;
  profiler_res_t **result;
  guint bin_size;
  GMutex mtx;             // prevent simultaneous write to progress
  guint64 *progress;
  gpointer other_data;
} prof_mt_params_t;


/**
 * this funciton performs cache_size/bin_size simulations to obtain miss ratio,
 * the size of simulations are from 0, bin_size, bin_size*2 ... cache_size,
 * it returns an array of profiler_res_t*, each element of the array is the result of one simulation
 * the user is responsible for g_free the memory of returned results
 * @param reader_in
 * @param cache_in
 * @param num_of_threads
 * @param bin_size
 * @return an array of profiler_res_t*, each element of the array is the result of one simulation
 */
profiler_res_t **evaluate(reader_t *reader_in, struct cache *cache_in, int num_of_threads, guint64 bin_size);



#ifdef __cplusplus
}
#endif

#endif /* profiler_h */

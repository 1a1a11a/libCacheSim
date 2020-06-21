//
//  generalProfiler.h
//  generalProfiler
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef generalProfiler_h
#define generalProfiler_h

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
  long long hit_count;        // this can be negative!!
  long long miss_count;
  float miss_ratio;
  float hit_ratio;
  double obj_hit_ratio;
  double byte_hit_ratio;
  long long cache_size;
  gpointer other_data;
} profiler_res_t;


typedef enum {
  P_HIT_RATIO = 0,
  P_EVICTION_AGE,
  P_HIT_RESULT,
  P_EVICTION,
} profiler_t;

struct multithreading_params_generalProfiler {
  reader_t *reader;
  struct cache *cache;
  profiler_res_t **result;
  guint bin_size;
  GHashTable *prefetch_hashtable;
  GMutex mtx;             // prevent simultaneous write to progress
  guint64 *progress;
  gpointer other_data;
};
typedef struct multithreading_params_generalProfiler mt_param_gp_t;


profiler_res_t **
profiler(reader_t *reader_in, struct cache *cache_in, int num_of_threads_in, int bin_size_in, profiler_t prof_type);

//gdouble *LRU_evict_err_statistics(
//  reader_t *reader_in,
//  cache_t *cache_in,
//  guint64 time_interval
//);


#ifdef __cplusplus
}
#endif

#endif /* generalProfiler_h */

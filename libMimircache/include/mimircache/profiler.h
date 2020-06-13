//
//  generalProfiler.h
//  generalProfiler
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef profiler_h
#define profiler_h

#ifdef __cplusplus
extern "C"
{
#endif


#include "reader.h"
#include "cache.h"


typedef struct {
  guint64 req_cnt;
  guint64 req_bytes;
  guint64 miss_cnt;
  guint64 miss_bytes;
  gint64 cache_size;
  cache_state_t cache_state;
  gpointer other_data;
} profiler_res_t;


typedef enum {
  P_MISS_RATIO = 0,
  P_EVICTION_AGE,
  P_HIT_RESULT,
  P_EVICTION,
} profiler_t;


/**
 *
 * this function performs num_of_sizes simulations to obtain miss ratio,
 * the cache size of simulations are from cache_sizes,
 * it returns an array of profiler_res_t*, each element of the array is the result of one simulation
 * the user is responsible for g_free the memory of returned results

 * @param reader
 * @param cache
 * @param num_of_sizes
 * @param cache_sizes
 * @param warmup_reader
 * @param warmup_perc
 * @param num_of_threads
 * @return
 */
profiler_res_t *
get_miss_ratio_curve(reader_t *const reader,
                     const cache_t *const cache,
                     const gint num_of_sizes,
                     const guint64 *const cache_sizes,
                     reader_t *const warmup_reader,
                     const double warmup_perc,
                     const gint num_of_threads);


/**
 * this function performs cache_size/step_size simulations to obtain miss ratio,
 * the size of simulations are step_size, step_size*2 ... step_size*n,
 * it returns an array of profiler_res_t*, each element of the array is the result of one simulation
 * the user is responsible for g_free the memory of returned results
 * @param reader_in
 * @param cache_in
 * @param step_size
 * @param warmup_perc
 * @param num_of_threads
 * @return an array of profiler_res_t*, each element of the array is the result of one simulation
 */

profiler_res_t *
get_miss_ratio_curve_with_step_size(reader_t *const reader_in,
                                    const cache_t *const cache_in,
                                    const guint64 step_size,
                                    reader_t *const warmup_reader,
                                    const double warmup_perc,
                                    const gint num_of_threads);


#ifdef __cplusplus
}
#endif

#endif /* profiler_h */

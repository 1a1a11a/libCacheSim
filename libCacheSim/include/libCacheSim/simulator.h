//
//  simulator.h
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef simulator_h
#define simulator_h

#ifdef __cplusplus
extern "C" {
#endif

#include "cache.h"
#include "reader.h"

typedef struct {
  uint64_t req_cnt;
  uint64_t req_bytes;
  uint64_t miss_cnt;
  uint64_t miss_bytes;
  uint64_t cache_size;
  cache_stat_t cache_state;
  void *other_data;
} sim_res_t;

typedef enum {
  MISS_RATIO = 0,
  EVICTION_AGE,
  HIT_RESULT,
  EVICTION,
} sim_type_e;

/**
 *
 * this function performs num_of_sizes simulations to obtain miss ratio,
 * the cache size of simulations are from cache_sizes,
 * it returns an array of simulator_res_t*, each element of the array is the
 result of one simulation
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
sim_res_t *
get_miss_ratio_curve(reader_t *const reader, const cache_t *const cache,
                     const gint num_of_sizes, const guint64 *const cache_sizes,
                     reader_t *const warmup_reader, const double warmup_perc,
                     const gint num_of_threads);

/**
 * this function performs cache_size/step_size simulations to obtain miss ratio,
 * the size of simulations are step_size, step_size*2 ... step_size*n,
 * it returns an array of simulator_res_t*, each element of the array is the
 * result of one simulation the user is responsible for g_free the memory of
 * returned results
 * @param reader_in
 * @param cache_in
 * @param step_size
 * @param warmup_perc
 * @param num_of_threads
 * @return an array of simulator_res_t*, each element of the array is the result
 * of one simulation
 */

sim_res_t *get_miss_ratio_curve_with_step_size(reader_t *const reader_in,
                                               const cache_t *const cache_in,
                                               const guint64 step_size,
                                               reader_t *const warmup_reader,
                                               const double warmup_perc,
                                               const gint num_of_threads);

#ifdef __cplusplus
}
#endif

#endif /* simulator_h */

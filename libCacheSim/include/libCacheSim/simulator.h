//
//  simulator.h
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef simulator_h
#define simulator_h

#include "cache.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * this function performs num_of_sizes simulations each at one cache size,
 * it returns an array of cache_stat_t*, each element is the result of one
 * simulation the returned cache_stat_t should be freed by the user
 *
 * this also supports warmup using
 *      a different trace by setting warmup_reader pointing to the trace
 *              or
 *      fraction of the requests from the reader
 *              or
 *      warmup_sec of requests from the reader
 *
 * @param reader
 * @param cache
 * @param num_of_sizes
 * @param cache_sizes
 * @param warmup_reader
 * @param warmup_frac
 * @param num_of_threads
 * @return
 */
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache,
                                      int num_of_sizes,
                                      const uint64_t *cache_sizes,
                                      reader_t *warmup_reader,
                                      double warmup_frac, int warmup_sec,
                                      int num_of_threads, bool use_random_seed);

/**
 * this function performs cache_size/step_size simulations to obtain miss ratio,
 * the size of simulations are step_size, step_size*2 ... step_size*n,
 * it returns an array of cache_stat_t*, each element of the array is the
 * result of one simulation
 * the returned cache_stat_t should be freed by the user
 *
 *  this also supports warmup using
 *   a different trace by setting warmup_reader pointing to the trace
 *           or
 *   fraction of the requests in the given trace reader by setting warmup_frac
 *
 * @param reader_in
 * @param cache_in
 * @param step_size
 * @param warmup_frac
 * @param num_of_threads
 * @return an array of cache_stat_t, each corresponds to one simulation
 */

cache_stat_t *simulate_at_multi_sizes_with_step_size(
    reader_t *reader_in, const cache_t *cache_in, uint64_t step_size,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool use_random_seed);

/**
 * this function performs num_of_caches simulations with the caches,
 * it returns a cache_stat_t
 * the returned cache_stat_t should be freed by the user
 * *
 * @param reader
 * @param caches
 * @param num_of_caches
 * @param warmup_reader
 * @param warmup_frac
 * @param num_of_threads
 * @return
 */
cache_stat_t *simulate_with_multi_caches(
    reader_t *reader, cache_t *caches[], int num_of_caches,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool free_cache_when_finish, bool use_random_seed);

#ifdef __cplusplus
}
#endif

#endif /* simulator_h */

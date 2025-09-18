/**
 * @file simulator.h
 * @brief Declares high-level functions for running cache simulations.
 *
 * This file provides the main entry points for running cache simulations.
 * It supports running simulations for multiple cache sizes, with different
 * warmup strategies, and utilizing multiple threads for parallel execution.
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "cache.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runs simulations for a given cache configuration at multiple cache sizes.
 *
 * This function performs parallel simulations for each specified cache size.
 * It supports warming up the caches using either a separate trace file or a fraction
 * of the main trace.
 *
 * @param reader The trace reader for the main simulation phase.
 * @param cache A template cache configuration to be cloned for each simulation.
 * @param num_of_sizes The number of cache sizes to simulate.
 * @param cache_sizes An array of cache sizes in bytes.
 * @param warmup_reader An optional trace reader for the warmup phase. Can be NULL.
 * @param warmup_frac The fraction of the main trace to use for warmup (e.g., 0.2 for 20%).
 *                    Used if warmup_reader is NULL.
 * @param warmup_sec The duration in seconds from the beginning of the trace to use for warmup.
 * @param num_of_threads The number of threads to use for parallel simulation.
 * @param use_random_seed If true, uses a random seed for simulations; otherwise, uses a fixed seed.
 * @return An array of `cache_stat_t` pointers, one for each simulation. The caller is
 *         responsible for freeing this array and the `cache_stat_t` objects within it.
 */
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache,
                                      int num_of_sizes,
                                      const uint64_t *cache_sizes,
                                      reader_t *warmup_reader,
                                      double warmup_frac, int warmup_sec,
                                      int num_of_threads, bool use_random_seed);

/**
 * @brief Runs simulations for a range of cache sizes defined by a step size.
 *
 * This function performs simulations for cache sizes: step_size, 2*step_size, ..., n*step_size
 * up to the working set size of the trace.
 *
 * @param reader_in The trace reader for the simulation.
 * @param cache_in A template cache configuration.
 * @param step_size The increment for cache sizes between simulations.
 * @param warmup_reader An optional trace reader for the warmup phase.
 * @param warmup_frac The fraction of the main trace to use for warmup.
 * @param warmup_sec The duration in seconds from the beginning of the trace to use for warmup.
 * @param num_of_threads The number of threads to use.
 * @param use_random_seed If true, uses a random seed.
 * @return An array of `cache_stat_t` pointers. The caller must free this array.
 */
cache_stat_t *simulate_at_multi_sizes_with_step_size(
    reader_t *reader_in, const cache_t *cache_in, uint64_t step_size,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool use_random_seed);

/**
 * @brief Runs simulations for multiple different cache configurations simultaneously.
 *
 * This is useful for comparing the performance of different cache algorithms in a single run.
 *
 * @param reader The trace reader.
 * @param caches An array of pointers to pre-initialized cache configurations.
 * @param num_of_caches The number of cache configurations in the `caches` array.
 * @param warmup_reader An optional trace reader for the warmup phase.
 * @param warmup_frac The fraction of the main trace to use for warmup.
 * @param warmup_sec The duration in seconds from the beginning of the trace to use for warmup.
 * @param num_of_threads The number of threads to use.
 * @param free_cache_when_finish If true, the cache objects will be freed by the function upon completion.
 * @param use_random_seed If true, uses a random seed.
 * @return An array of `cache_stat_t` pointers, one for each cache configuration. The caller must free this array.
 */
cache_stat_t *simulate_with_multi_caches(
    reader_t *reader, cache_t *caches[], int num_of_caches,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool free_cache_when_finish, bool use_random_seed);

#ifdef __cplusplus
}
#endif

#endif /* SIMULATOR_H */

/**
 * @file profilerLRU.h
 * @brief Provides functions for efficiently profiling LRU cache performance.
 *
 * This file contains functions to calculate the miss ratio for an LRU cache
 * without needing to run a full, slow simulation. It achieves this by using
 * stack distance analysis, which is a highly efficient method specifically
 * for LRU-like policies.
 */

#ifndef profilerLRU_h
#define profilerLRU_h

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "const.h"
#include "dist.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculates the object miss ratio for an LRU cache of a given size.
 *
 * @param reader The trace reader.
 * @param size The size of the LRU cache to profile.
 * @return A pointer to a double containing the miss ratio. The caller is
 *         responsible for freeing this memory.
 */
double *get_lru_obj_miss_ratio(reader_t *reader, gint64 size);

/**
 * @brief Calculates the object miss ratio curve for an LRU cache.
 *
 * This function computes the miss ratio for a range of cache sizes, producing
 * a miss ratio curve (MRC).
 *
 * @param reader The trace reader.
 * @param size The maximum cache size for the curve.
 * @return An array of doubles representing the miss ratio at different sizes.
 *         The caller is responsible for freeing this array.
 */
double *get_lru_obj_miss_ratio_curve(reader_t *reader, gint64 size);

/**
 * @brief (Internal) Calculates the raw miss count for an LRU cache.
 *
 * This is an internal helper function used by the miss ratio functions. It
 * computes the number of misses for an LRU cache of a given size.
 *
 * @param reader The trace reader.
 * @param size The size of the LRU cache.
 * @return A pointer to an int64_t containing the total number of misses. The
 *         caller is responsible for freeing this memory.
 */
int64_t *_get_lru_miss_cnt(reader_t *reader, int64_t size);

#ifdef __cplusplus
}
#endif

#endif /* profilerLRU_h */

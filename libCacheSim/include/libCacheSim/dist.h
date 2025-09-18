/**
 * @file dist.h
 * @brief Provides functions for calculating, saving, and loading trace distances.
 *
 * This file contains utilities to compute various types of distances for each
 * request in a trace. These distances, such as stack distance (reuse distance)
 * or time since last access, are crucial for certain types of cache analysis
 * and for some eviction algorithms like Belady's.
 */

#ifndef libCacheSim_DISTUTILS_H
#define libCacheSim_DISTUTILS_H

#include "const.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumerates the different types of distances that can be calculated.
 */
typedef enum {
  DIST_SINCE_LAST_ACCESS,   /**< The number of requests since the last access to the same object. */
  DIST_SINCE_FIRST_ACCESS,  /**< The number of requests since the first access to the same object. */
  STACK_DIST,               /**< The number of unique objects seen since the last access to the same object. */
  FUTURE_STACK_DIST,        /**< The number of unique objects that will be seen until the next access to the same object. */
} dist_type_e;

/**
 * @brief String representations for the dist_type_e enum.
 */
static const char *g_dist_type_name[] = {
    "DIST_SINCE_LAST_ACCESS",
    "DIST_SINCE_FIRST_ACCESS",
    "STACK_DIST",
    "FUTURE_STACK_DIST",
};

/**
 * @brief Gets the stack distance for each request in a trace.
 *
 * Stack distance (or reuse distance) is the number of unique objects seen
 * between consecutive accesses to the same object. Future stack distance
 * looks forward instead of backward. This requires a full pass over the trace.
 *
 * @param reader The trace reader, positioned at the beginning of the trace.
 * @param dist_type The type of stack distance to compute (STACK_DIST or FUTURE_STACK_DIST).
 * @param array_size A pointer to a variable that will be filled with the size of the returned array.
 * @return An array of `int32_t` with the computed distance for each request. The
 *         caller is responsible for freeing this array.
 */
int32_t *get_stack_dist(reader_t *reader, const dist_type_e dist_type,
                        int64_t *array_size);

/**
 * @brief Gets the access distance for each request in a trace.
 *
 * Access distance is the number of requests (not unique objects) seen since
 * a previous access to the same object. This requires a full pass over the trace.
 *
 * @param reader The trace reader, positioned at the beginning of the trace.
 * @param dist_type The type of access distance to compute (DIST_SINCE_LAST_ACCESS or DIST_SINCE_FIRST_ACCESS).
 * @param array_size A pointer to a variable that will be filled with the size of the returned array.
 * @return An array of `int32_t` with the computed distance for each request. The
 *         caller is responsible for freeing this array.
 */
int32_t *get_access_dist(reader_t *reader, const dist_type_e dist_type,
                         int64_t *array_size);

/**
 * @brief Saves a distance array to a file in a binary format.
 *
 * This allows pre-computed distances to be reused without recalculating them.
 *
 * @param reader The trace reader (used for metadata).
 * @param dist_array The array of distances to save.
 * @param array_size The size of the distance array.
 * @param ofilepath The path to the output file.
 * @param dist_type The type of distance being saved.
 */
void save_dist(reader_t *const reader, const int32_t *dist_array,
               const int64_t array_size, const char *const ofilepath,
               const dist_type_e dist_type);

/**
 * @brief Saves a distance array to a file in a text format.
 *
 * @param reader The trace reader (used for metadata).
 * @param dist_array The array of distances to save.
 * @param array_size The size of the distance array.
 * @param ofilepath The path to the output file.
 * @param dist_type The type of distance being saved.
 */
void save_dist_txt(reader_t *const reader, const int32_t *dist_array,
                   const int64_t array_size, const char *const ofilepath,
                   const dist_type_e dist_type);

/**
 * @brief Loads a pre-computed distance array from a file.
 *
 * @param reader The trace reader (used for metadata).
 * @param ifilepath The path to the input distance file.
 * @param array_size A pointer to a variable that will be filled with the size of the loaded array.
 * @return An array of `int32_t` with the loaded distances. The caller is
 *         responsible for freeing this array.
 */
int32_t *load_dist(reader_t *const reader, const char *const ifilepath,
                   int64_t *array_size);

/**
 * @brief Saves a distance array as a frequency count in text format.
 *
 * Instead of writing one line per request, this function computes a histogram
 * of the distances and writes the counts to the output file.
 *
 * @param reader The trace reader (used for metadata).
 * @param dist_array The array of distances.
 * @param array_size The size of the distance array.
 * @param ofilepath The path to the output file.
 * @param dist_type The type of distance being saved.
 */
void save_dist_as_cnt_txt(reader_t *const reader, const int32_t *dist_array,
                          const int64_t array_size, const char *const ofilepath,
                          const dist_type_e dist_type);

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_DISTUTILS_H

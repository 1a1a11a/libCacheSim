/**
 * @file prefetchAlgo.h
 * @brief Defines the interface and structures for cache prefetching algorithms.
 *
 * Prefetching algorithms attempt to predict future requests and fetch data into
 * the cache before it is explicitly requested, with the goal of reducing miss
 * latency. This file defines the `prefetcher_t` structure, which encapsulates
 * the logic for a prefetching policy.
 */

#ifndef PREFETCHINGALGO_H
#define PREFETCHINGALGO_H

#include <strings.h>

#include "cache.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prefetcher;
struct cache;

/** @brief Function pointer to create and initialize a prefetcher. */
typedef struct prefetcher *(*prefetcher_create_func_ptr)(const char *);

/** @brief Function pointer to trigger a prefetch based on a request. */
typedef void (*prefetcher_prefetch_func_ptr)(struct cache *, const request_t *);

/** @brief Function pointer to handle a cache find event. */
typedef void (*prefetcher_handle_find_func_ptr)(struct cache *, const request_t *, bool);

/** @brief Function pointer to handle a cache insert event. */
typedef void (*prefetcher_handle_insert_func_ptr)(struct cache *, const request_t *);

/** @brief Function pointer to handle a cache evict event. */
typedef void (*prefetcher_handle_evict_func_ptr)(struct cache *, const request_t *);

/** @brief Function pointer to free a prefetcher. */
typedef void (*prefetcher_free_func_ptr)(struct prefetcher *);

/** @brief Function pointer to clone a prefetcher. */
typedef struct prefetcher *(*prefetcher_clone_func_ptr)(struct prefetcher *, uint64_t);

/**
 * @brief The main structure for a cache prefetching policy.
 *
 * This structure holds the function pointers and parameters that define the
 * behavior of a prefetcher. It can react to various cache events (find, insert, evict)
 * to make prefetching decisions.
 */
typedef struct prefetcher {
  prefetcher_prefetch_func_ptr prefetch;         /**< Main function to initiate prefetching. */
  prefetcher_handle_find_func_ptr handle_find;   /**< Callback for when an object is looked up. */
  prefetcher_handle_insert_func_ptr handle_insert; /**< Callback for when an object is inserted. */
  prefetcher_handle_evict_func_ptr handle_evict;   /**< Callback for when an object is evicted. */
  prefetcher_free_func_ptr free;                 /**< Function to free the prefetcher. */
  prefetcher_clone_func_ptr clone;               /**< Function to clone the prefetcher. */
  void *params;                                  /**< A pointer to algorithm-specific parameters. */
  char *init_params;                             /**< The initialization parameter string. */
  char prefetcher_name[64];                      /**< The name of the prefetching algorithm. */
} prefetcher_t;

// Creation functions for specific prefetching algorithms
prefetcher_t *create_Mithril_prefetcher(const char *init_params, uint64_t cache_size);
prefetcher_t *create_OBL_prefetcher(const char *init_params, uint64_t cache_size);
prefetcher_t *create_PG_prefetcher(const char *init_params, uint64_t cache_size);

/**
 * @brief A factory function to create a prefetcher based on a name.
 *
 * @param prefetching_algo The name of the prefetching algorithm (e.g., "Mithril", "OBL").
 * @param prefetching_params A string containing algorithm-specific parameters.
 * @param cache_size The size of the cache, which may be needed by the prefetcher.
 * @return A pointer to a newly created `prefetcher_t` instance, or NULL if the
 *         algorithm name is not recognized.
 */
static inline prefetcher_t *create_prefetcher(const char *prefetching_algo,
                                              const char *prefetching_params,
                                              uint64_t cache_size) {
  prefetcher_t *prefetcher = NULL;
  if (strcasecmp(prefetching_algo, "Mithril") == 0) {
    prefetcher = create_Mithril_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "OBL") == 0) {
    prefetcher = create_OBL_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "PG") == 0) {
    prefetcher = create_PG_prefetcher(prefetching_params, cache_size);
  } else {
    ERROR("prefetching algo %s not supported\n", prefetching_algo);
  }

  return prefetcher;
}

#ifdef __cplusplus
}
#endif
#endif

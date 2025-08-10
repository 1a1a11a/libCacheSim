/**
 * @file plugin.h
 * @brief Plugin API for libCacheSim cache implementations
 *
 * This header defines two plugin APIs for implementing custom cache algorithms:
 * - v1 API: Full cache implementation using libCacheSim data structures
 * - v2 API: Hook-based implementation for easy integration with existing caches
 *
 * @author Juncheng Yang
 * @date Created on 11/17/19
 */

#ifndef libCacheSim_PLUGIN_H
#define libCacheSim_PLUGIN_H

#include "cache.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   V1 Plugin API                              ****
// ****                                                               ****
// ***********************************************************************

/**
 * @defgroup v1_plugin_api V1 Plugin API
 * @brief Full cache implementation API using libCacheSim data structures
 *
 * The v1 plugin API requires implementing a complete cache using libCacheSim
 * data structures similar to the built-in LRU implementation. This provides
 * the lowest overhead and full integration with libCacheSim.
 * @{
 */

/**
 * @brief Create a cache handler using a built-in cache replacement algorithm
 *
 * Creates a cache instance using one of the cache replacement algorithms
 * that are compiled into libCacheSim.
 *
 * @param cache_alg_name Name of the cache replacement algorithm (case
 * sensitive)
 * @param cc_params General cache parameters (cache_size,
 * support_ttl, etc.)
 * @param cache_specific_params Algorithm-specific parameters (can be NULL)
 * @return Pointer to initialized cache handler, or NULL on failure
 */
cache_t *create_cache_using_plugin(const char *const cache_alg_name,
                                   common_cache_params_t cc_params,
                                   void *cache_specific_params);

/**
 * @brief Internal cache creation function
 *
 * Similar to create_cache_using_plugin() but for internal use within
 * libCacheSim.
 *
 * @param cache_alg_name Name of the cache replacement algorithm (case
 * sensitive)
 * @param cc_params General cache parameters
 * @param cache_specific_params Algorithm-specific parameters (can be NULL)
 * @return Pointer to initialized cache handler, or NULL on failure
 */
cache_t *create_cache_internal(const char *const cache_alg_name,
                               common_cache_params_t cc_params,
                               void *cache_specific_params);

/**
 * @brief Create a cache handler using an external cache replacement algorithm
 *
 * Creates a cache instance using an external cache replacement algorithm
 * compiled into a shared library. The library should be named <alg>.so
 * where <alg> is the algorithm name passed to this function.
 *
 * @param cache_alg_name Name of the cache replacement algorithm (case
 * sensitive)
 * @param cc_params General cache parameters
 * @param cache_specific_params Algorithm-specific parameters (can be NULL)
 * @return Pointer to initialized cache handler, or NULL on failure
 *
 * @note The shared library must be in the working directory
 */
cache_t *create_cache_external(const char *const cache_alg_name,
                               common_cache_params_t cc_params,
                               void *cache_specific_params);

/** @} */  // end of v1_plugin_api group

// ***********************************************************************
// ****                                                               ****
// ****                   V2 Plugin Cache API                        ****
// ****                                                               ****
// ***********************************************************************

/**
 * @defgroup v2_plugin_api V2 Plugin Cache API
 * @brief Hook-based plugin API for easy cache integration
 *
 * The v2 plugin cache API allows implementing a cache plugin by providing
 * five core hook functions. This design enables easy integration on top of
 * existing cache implementations without requiring full reimplementation.
 *
 * Required hooks:
 * - cache_init_hook: Initialize plugin data structures
 * - cache_hit_hook: Handle cache hit events
 * - cache_miss_hook: Handle cache miss events
 * - cache_eviction_hook: Determine which object to evict
 * - cache_remove_hook: Clean up when objects are removed
 * - cache_free_hook: Free plugin resources
 *
 * @see example/plugin_v2/plugin_lru.c for a complete implementation example
 * @{
 */

/**
 * @brief Cache initialization hook function type
 *
 * This function is called during cache initialization to set up any
 * plugin-specific data structures. The plugin should allocate and
 * initialize any internal state needed for cache operations.
 *
 * @param ccache_params Cache configuration parameters
 * @return Pointer to plugin's internal data structure, or NULL on failure
 *
 * @note The returned pointer will be passed to all other hook functions
 */
typedef void *(*cache_init_hook_t)(const common_cache_params_t ccache_params);

/**
 * @brief Cache hit hook function type
 *
 * Called when a requested object is found in the cache. The plugin should
 * perform any necessary bookkeeping operations, such as updating access
 * order for LRU-based algorithms.
 *
 * @param data Pointer to plugin's internal data (from cache_init_hook)
 * @param req The cache request being processed
 *
 * @note This function should not modify the cache contents, only metadata
 */
typedef void (*cache_hit_hook_t)(void *data, const request_t *req);

/**
 * @brief Cache miss hook function type
 *
 * Called when a requested object is not found in the cache. The plugin
 * should perform any necessary bookkeeping for the new object that will
 * be inserted, such as adding it to tracking data structures.
 *
 * @param data Pointer to plugin's internal data (from cache_init_hook)
 * @param req The cache request being processed
 *
 * @note The actual object insertion is handled by the cache framework
 */
typedef void (*cache_miss_hook_t)(void *data, const request_t *req);

/**
 * @brief Cache eviction hook function type
 *
 * Called when the cache is full and an object must be evicted to make space.
 * The plugin should determine which object to evict based on its replacement
 * policy and return the object ID.
 *
 * @param data Pointer to plugin's internal data (from cache_init_hook)
 * @param req The cache request currently being processed
 * @return Object ID of the object to be evicted
 *
 * @warning The returned object ID must correspond to a valid object in the
 * cache
 */
typedef obj_id_t (*cache_eviction_hook_t)(void *data, const request_t *req);

/**
 * @brief Cache removal hook function type
 *
 * Called when an object is being removed from the cache (either due to
 * eviction or explicit deletion). The plugin should clean up any internal
 * state associated with the object.
 *
 * @param data Pointer to plugin's internal data (from cache_init_hook)
 * @param obj_id Object ID of the object being removed
 *
 * @note This is called after the object has been removed from the cache
 */
typedef void (*cache_remove_hook_t)(void *data, const obj_id_t obj_id);

/**
 * @brief Cache free hook function type
 *
 * Cleanup function called when the cache is being destroyed.
 * The plugin should free any resources allocated in cache_init_hook.
 *
 * @param data Pointer to plugin's internal data (from cache_init_hook)
 */
typedef void (*cache_free_hook_t)(void *data);

/** @} */  // end of v2_plugin_api group

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_PLUGIN_H

/**
 * @file plugin_cache.c
 * @brief Plugin cache implementation for libCacheSim
 *
 * This file implements a plugin-based cache that allows users to provide custom
 * cache replacement algorithms via shared libraries. The plugin cache provides
 * hooks for cache hit, cache miss, eviction, and removal, which are loaded at
 * runtime from the user-specified plugin.
 *
 * The plugin must implement the following hook functions:
 *   - cache_init_hook: Initialize plugin data structures
 *   - cache_hit_hook: Handle cache hit events
 *   - cache_miss_hook: Handle cache miss events
 *   - cache_eviction_hook: Determine which object to evict
 *   - cache_remove_hook: Clean up when objects are removed
 *   - cache_free_hook: Free plugin resources
 *
 * The plugin cache delegates core cache operations to these hooks, enabling
 * flexible and extensible cache policies.
 *
 * @author Juncheng Yang
 * @date Created in 2025
 * @copyright Copyright © 2025 Juncheng. All rights reserved.
 */

#include <dlfcn.h>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                    Data Structures                           ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief Plugin cache parameters structure
 *
 * Contains all plugin-specific data including the plugin path, loaded function
 * pointers, and internal plugin data.
 */
typedef struct pluginCache_params {
  char *plugin_path;                  ///< Path to the plugin shared library
  void *plugin_handle;                ///< Handle to the loaded plugin library
  void *data;                         ///< Plugin's internal data structure
  cache_init_hook_t cache_init_hook;  ///< Plugin initialization function
  cache_hit_hook_t cache_hit_hook;    ///< Cache hit handler function
  cache_miss_hook_t cache_miss_hook;  ///< Cache miss handler function
  cache_eviction_hook_t cache_eviction_hook;  ///< Eviction decision function
  cache_remove_hook_t cache_remove_hook;  ///< Object removal handler function
  cache_free_hook_t cache_free_hook;      ///< Cache cleanup function
  char *cache_name;
} pluginCache_params_t;

/** @brief Default plugin parameters if none specified */
static const char *DEFAULT_CACHE_PARAMS =
    "plugin_path=./libplugin_lru_hooks.so,cache_name=pluginCache";

// ***********************************************************************
// ****                                                               ****
// ****                    Function Declarations                     ****
// ****                                                               ****
// ***********************************************************************

static void pluginCache_parse_params(cache_t *cache,
                                     const char *cache_specific_params);
static void pluginCache_free(cache_t *cache);
static bool pluginCache_get(cache_t *cache, const request_t *req);
static cache_obj_t *pluginCache_find(cache_t *cache, const request_t *req,
                                     const bool update_cache);
static cache_obj_t *pluginCache_insert(cache_t *cache, const request_t *req);
static cache_obj_t *pluginCache_to_evict(cache_t *cache, const request_t *req);
static void pluginCache_evict(cache_t *cache, const request_t *req);
static bool pluginCache_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                    Public API Functions                      ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief Initialize a plugin cache instance
 *
 * Creates and initializes a plugin-based cache by loading the specified plugin
 * shared library and setting up the hook functions. The plugin must export the
 * required hook functions for proper operation.
 *
 * @param ccache_params Common cache parameters (size, TTL, etc.)
 * @param cache_specific_params Plugin-specific parameters string (format:
 * "key=value,key=value") Must include "plugin_path" parameter
 * @return Pointer to initialized cache instance, or NULL on failure
 *
 * @note The plugin shared library must be accessible and export all required
 * hook functions
 * @see pluginCache_parse_params() for parameter format details
 */
cache_t *pluginCache_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params) {
  // Initialize base cache structure
  cache_t *cache =
      cache_struct_init("pluginCache", ccache_params, cache_specific_params);

  // Set function pointers for cache operations
  cache->cache_init = pluginCache_init;
  cache->cache_free = pluginCache_free;
  cache->get = pluginCache_get;
  cache->find = pluginCache_find;
  cache->insert = pluginCache_insert;
  cache->evict = pluginCache_evict;
  cache->remove = pluginCache_remove;
  cache->to_evict = pluginCache_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = cache_can_insert_default;
  cache->obj_md_size = 0;

  // Allocate and initialize plugin parameters
  cache->eviction_params =
      (pluginCache_params_t *)malloc(sizeof(pluginCache_params_t));
  pluginCache_params_t *params =
      (pluginCache_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(pluginCache_params_t));

  // Parse parameters (default first, then user-specified)
  pluginCache_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    pluginCache_parse_params(cache, cache_specific_params);
  }

  // Load the plugin shared library
  void *handle = dlopen(params->plugin_path, RTLD_NOW);
  if (handle == NULL) {
    ERROR("Failed to load plugin %s: %s\n", params->plugin_path, dlerror());
    exit(1);
  }
  params->plugin_handle = handle;

  // Load hook functions from the plugin using unions to avoid pedantic warnings
  union {
    void *obj;
    cache_init_hook_t func;
  } cache_init_u;
  union {
    void *obj;
    cache_hit_hook_t func;
  } cache_hit_u;
  union {
    void *obj;
    cache_miss_hook_t func;
  } cache_miss_u;
  union {
    void *obj;
    cache_eviction_hook_t func;
  } cache_eviction_u;
  union {
    void *obj;
    cache_remove_hook_t func;
  } cache_remove_u;
  union {
    void *obj;
    cache_free_hook_t func;
  } cache_free_u;

  cache_init_u.obj = dlsym(handle, "cache_init_hook");
  params->cache_init_hook = cache_init_u.func;

  cache_hit_u.obj = dlsym(handle, "cache_hit_hook");
  params->cache_hit_hook = cache_hit_u.func;

  cache_miss_u.obj = dlsym(handle, "cache_miss_hook");
  params->cache_miss_hook = cache_miss_u.func;

  cache_eviction_u.obj = dlsym(handle, "cache_eviction_hook");
  params->cache_eviction_hook = cache_eviction_u.func;

  cache_remove_u.obj = dlsym(handle, "cache_remove_hook");
  params->cache_remove_hook = cache_remove_u.func;

  cache_free_u.obj = dlsym(handle, "cache_free_hook");
  params->cache_free_hook = cache_free_u.func;

  // Initialize the plugin with cache parameters
  params->data = params->cache_init_hook(ccache_params);

  if (strcmp(params->cache_name, "pluginCache") != 0) {
    snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "%s", params->cache_name);
  } else {
    // Set cache name to include plugin name for identification
    char *plugin_name = strrchr(params->plugin_path, '/');
    if (plugin_name == NULL) {
      plugin_name = params->plugin_path;
    } else {
      plugin_name++;
    }
    snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "pluginCache-%s",
             plugin_name);
  }

  return cache;
}

/**
 * @brief Free resources used by the plugin cache
 *
 * Cleans up all resources allocated by the plugin cache, including
 * plugin-specific parameters and base cache structures.
 *
 * @param cache Pointer to the cache instance to free
 */
static void pluginCache_free(cache_t *cache) {
  pluginCache_params_t *params = (pluginCache_params_t *)cache->eviction_params;

  if (params->cache_free_hook != NULL) params->cache_free_hook(params->data);
  if (params->plugin_handle != NULL)
    dlclose(params->plugin_handle);  // Close the plugin shared library handle
  if (params->plugin_path != NULL) free(params->plugin_path);
  if (params->cache_name != NULL) free(params->cache_name);
  free(cache->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief Main cache access function (user-facing API)
 *
 * Implements the standard cache access pattern:
 * 1. Check if object exists in cache
 * 2. If hit: call plugin hit hook and return true
 * 3. If miss: evict objects if needed, insert new object, call plugin miss
 * hook, return false
 *
 * @param cache Pointer to the cache instance
 * @param req Cache request containing object ID, size, and timestamp
 * @return true if cache hit, false if cache miss
 */
static bool pluginCache_get(cache_t *cache, const request_t *req) {
  bool hit = cache_get_base(cache, req);
  pluginCache_params_t *params = (pluginCache_params_t *)cache->eviction_params;

  if (hit) {
    params->cache_hit_hook(params->data, req);
  } else {
    params->cache_miss_hook(params->data, req);
  }

  return hit;
}

// ***********************************************************************
// ****                                                               ****
// ****                Developer-Facing Functions                    ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief Find an object in the cache
 *
 * Searches for an object in the cache and optionally updates cache metadata
 * such as access time or promotion in the replacement policy.
 *
 * @param cache Pointer to the cache instance
 * @param req Cache request containing the object to find
 * @param update_cache Whether to update cache metadata on access
 * @return Pointer to the cache object if found, NULL otherwise
 */
static cache_obj_t *pluginCache_find(cache_t *cache, const request_t *req,
                                     const bool update_cache) {
  return cache_find_base(cache, req, update_cache);
}

/**
 * @brief Insert an object into the cache
 *
 * Inserts a new object into the cache, updating the hash table and cache
 * metadata. This function assumes the cache has sufficient space; eviction
 * should be handled separately before calling this function.
 *
 * @param cache Pointer to the cache instance
 * @param req Cache request containing the object to insert
 * @return Pointer to the inserted cache object
 */
static cache_obj_t *pluginCache_insert(cache_t *cache, const request_t *req) {
  return cache_insert_base(cache, req);
}

/**
 * @brief Find the object to be evicted (not supported)
 *
 * This function is not supported by the plugin cache because eviction logic
 * is delegated to the plugin's eviction hook, which may have complex internal
 * state that cannot be easily queried.
 *
 * @param cache Pointer to the cache instance
 * @param req Current cache request (unused)
 * @return Never returns (calls exit)
 *
 * @note This function always terminates the program with an error
 */
static cache_obj_t *pluginCache_to_evict(cache_t *cache, const request_t *req) {
  ERROR("pluginCache does not support to_evict function\n");
  exit(1);
}

/**
 * @brief Evict an object from the cache
 *
 * Delegates eviction decision to the plugin's eviction hook, then removes
 * the selected object from the cache. Updates all cache metadata including
 * object count, occupied size, and hash table.
 *
 * @param cache Pointer to the cache instance
 * @param req Current cache request (passed to plugin for context)
 *
 * @note The plugin's eviction hook must return a valid object ID that exists in
 * the cache
 */
static void pluginCache_evict(cache_t *cache, const request_t *req) {
  pluginCache_params_t *params = (pluginCache_params_t *)cache->eviction_params;

  // Get eviction candidate from plugin
  obj_id_t obj_id = params->cache_eviction_hook(params->data, req);

  // Find the object in the cache
  cache_obj_t *obj_to_evict = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj_to_evict == NULL) {
    ERROR("pluginCache: object %" PRIu64 " to be evicted not found in cache\n",
          obj_id);
    exit(1);
  }

  // Perform the eviction
  cache_evict_base(cache, obj_to_evict, true);
}

/**
 * @brief Remove an object from the cache
 *
 * Removes a specific object from the cache, typically in response to an
 * explicit user request rather than automatic eviction. Notifies the plugin of
 * the removal and updates all cache metadata.
 *
 * @param cache Pointer to the cache instance
 * @param obj_id ID of the object to remove
 * @return true if the object was found and removed, false if not found
 */
static bool pluginCache_remove(cache_t *cache, const obj_id_t obj_id) {
  pluginCache_params_t *params = (pluginCache_params_t *)cache->eviction_params;

  // Notify plugin of the removal
  params->cache_remove_hook(params->data, obj_id);

  // Find the object in the cache
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  // Remove the object from the cache
  cache_remove_obj_base(cache, obj, true);
  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                    Parameter Parsing                         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief Parse plugin cache parameters from string
 *
 * Parses a parameter string in the format "key1=value1,key2=value2,..."
 * and updates the plugin cache configuration accordingly.
 *
 * Supported parameters:
 * - plugin_path: Path to the plugin shared library
 * - print: Print current parameters and exit (for debugging)
 *
 * @param cache Pointer to the cache instance
 * @param cache_specific_params Parameter string to parse
 *
 * @note Invalid parameters will cause the program to exit with an error
 */
static void pluginCache_parse_params(cache_t *cache,
                                     const char *cache_specific_params) {
  pluginCache_params_t *params =
      (pluginCache_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    // Parse key=value pairs separated by commas
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // Check if value is NULL
    if (value == NULL) {
      ERROR("Parameter '%s' is missing a value in cache '%s'\n", key,
            cache->cache_name);
      exit(1);
    }
    // Skip whitespace
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    // Process recognized parameters
    if (strcasecmp(key, "plugin") == 0 || strcasecmp(key, "plugin_path") == 0) {
      // Validate plugin path is not empty
      if (strlen(value) == 0) {
        ERROR("Parameter 'plugin_path' cannot be empty in cache '%s'\n",
              cache->cache_name);
        exit(1);
      }
      if (params->plugin_path != NULL) free(params->plugin_path);
      params->plugin_path = strdup(value);
    } else if (strcasecmp(key, "cache_name") == 0) {
      if (params->cache_name != NULL) free(params->cache_name);
      params->cache_name = strdup(value);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: plugin_path=%s\n", params->plugin_path);
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif

/**
 * @file S3FIFO.c
 * @brief Implementation of the Simple, Scalable, Scan-resistant (S3-FIFO) cache eviction algorithm.
 *
 * S3-FIFO is a recent algorithm that aims to achieve scan-resistance and high
 * performance with a simpler design than traditional complex algorithms like ARC.
 * It uses three queues:
 *
 * - **S (Small):** A small FIFO queue that acts as an admission filter. New objects
 *   are inserted here.
 * - **M (Main):** A larger queue for objects that have demonstrated some reuse.
 *   Objects are moved from S to M upon their first re-access. This queue uses
 *   a CLOCK-like mechanism with a 1-bit frequency counter for eviction.
 * - **G (Ghost):** A non-resident ghost queue that tracks recently evicted objects
 *   from S. If a new object is found in G, it is inserted directly into M,
 *   bypassing S.
 *
 * This implementation differs slightly from the original paper. When the small
 * queue is full but the overall cache is not, new items are inserted directly
 * into the main queue. This can improve performance in some scenarios.
 *
 * Based on the paper: "FIFO Queues are All You Need for Cache Eviction"
 * by Juncheng Yang, et al.
 * https://dl.acm.org/doi/10.1145/3600006.3613147
 */

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters specific to the S3-FIFO algorithm.
 */
typedef struct {
  cache_t *small_fifo;  /**< The 'S' (Small) queue. */
  cache_t *ghost_fifo;  /**< The 'G' (Ghost) queue. */
  cache_t *main_fifo;   /**< The 'M' (Main) queue. */
  bool hit_on_ghost;    /**< Flag to indicate if the current request was a hit in the ghost queue. */

  int move_to_main_threshold; /**< Number of hits in the small queue required to promote to main. */
  double small_size_ratio;    /**< The ratio of total cache size allocated to the small queue. */
  double ghost_size_ratio;    /**< The ratio of total cache size allocated to the ghost queue. */

  bool has_evicted;       /**< A flag to track eviction state. */
  request_t *req_local;   /**< A local request object for temporary use during eviction. */
} S3FIFO_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "small-size-ratio=0.10,ghost-size-ratio=0.90,move-to-main-threshold=1";

// Forward declarations for static functions
static void S3FIFO_free(cache_t *cache);
static bool S3FIFO_get(cache_t *cache, const request_t *req);
static cache_obj_t *S3FIFO_find(cache_t *cache, const request_t *req, const bool update_cache);
static cache_obj_t *S3FIFO_insert(cache_t *cache, const request_t *req);
static void S3FIFO_evict(cache_t *cache, const request_t *req);
static bool S3FIFO_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t S3FIFO_get_occupied_byte(const cache_t *cache);
static inline int64_t S3FIFO_get_n_obj(const cache_t *cache);
static void S3FIFO_parse_params(cache_t *cache, const char *cache_specific_params);
static void S3FIFO_evict_small(cache_t *cache, const request_t *req);
static void S3FIFO_evict_main(cache_t *cache, const request_t *req);

/**
 * @brief Initializes an S3-FIFO cache.
 *
 * @param ccache_params Common cache parameters.
 * @param cache_specific_params Algorithm-specific parameters (e.g., queue size ratios).
 * @return A pointer to the initialized cache_t structure.
 */
cache_t *S3FIFO_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("S3FIFO", ccache_params, cache_specific_params);
  cache->cache_init = S3FIFO_init;
  cache->cache_free = S3FIFO_free;
  cache->get = S3FIFO_get;
  cache->find = S3FIFO_find;
  cache->insert = S3FIFO_insert;
  cache->evict = S3FIFO_evict;
  cache->remove = S3FIFO_remove;
  cache->get_n_obj = S3FIFO_get_n_obj;
  cache->get_occupied_byte = S3FIFO_get_occupied_byte;

  cache->eviction_params = calloc(1, sizeof(S3FIFO_params_t));
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  params->req_local = new_request();

  S3FIFO_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    S3FIFO_parse_params(cache, cache_specific_params);
  }

  int64_t small_fifo_size = (int64_t)(ccache_params.cache_size * params->small_size_ratio);
  int64_t main_fifo_size = ccache_params.cache_size - small_fifo_size;
  int64_t ghost_fifo_size = (int64_t)(ccache_params.cache_size * params->ghost_size_ratio);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = small_fifo_size;
  params->small_fifo = FIFO_init(ccache_params_local, NULL);

  params->ghost_fifo = NULL;
  if (ghost_fifo_size > 0) {
    ccache_params_local.cache_size = ghost_fifo_size;
    params->ghost_fifo = FIFO_init(ccache_params_local, NULL);
  }

  ccache_params_local.cache_size = main_fifo_size;
  // The "main" queue is a CLOCK cache, not FIFO.
  params->main_fifo = Clock_init(ccache_params_local, "n_bit_counter=2");

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S3FIFO-%.2f", params->small_size_ratio);
  return cache;
}

/**
 * @brief Frees the resources used by the S3-FIFO cache.
 * @param cache The cache to free.
 */
static void S3FIFO_free(cache_t *cache) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->small_fifo->cache_free(params->small_fifo);
  if (params->ghost_fifo != NULL) {
    params->ghost_fifo->cache_free(params->ghost_fifo);
  }
  params->main_fifo->cache_free(params->main_fifo);
  free(params->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief Handles a get request for the S3-FIFO cache.
 * @param cache The cache.
 * @param req The request to process.
 * @return True if it was a cache hit, false otherwise.
 */
static bool S3FIFO_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

/**
 * @brief Finds an object in the S3-FIFO queues.
 *
 * It checks the small and main queues for a data hit. It also checks the ghost
 * queue to detect re-access of a recently evicted object.
 *
 * @param cache The cache.
 * @param req The request.
 * @param update_cache If true, update object metadata (like frequency bits).
 * @return A pointer to the cache object if a data hit occurred, otherwise NULL.
 */
static cache_obj_t *S3FIFO_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;

  if (!update_cache) {
    // Fast path for non-updating finds
    cache_obj_t *obj = params->small_fifo->find(params->small_fifo, req, false);
    if (obj) return obj;
    return params->main_fifo->find(params->main_fifo, req, false);
  }

  params->hit_on_ghost = false;

  // Check small queue
  cache_obj_t *obj = params->small_fifo->find(params->small_fifo, req, true);
  if (obj != NULL) {
    obj->S3FIFO.freq = 1; // Mark as re-accessed
    return obj;
  }

  // Check ghost queue
  if (params->ghost_fifo && params->ghost_fifo->remove(params->ghost_fifo, req->obj_id)) {
    params->hit_on_ghost = true;
  }

  // Check main queue
  obj = params->main_fifo->find(params->main_fifo, req, true);
  if (obj != NULL) {
    obj->S3FIFO.freq = MIN(3, obj->S3FIFO.freq + 1); // Increment frequency up to 3
  }

  return obj;
}

/**
 * @brief Inserts a new object into the cache.
 *
 * - If the object was a hit on the ghost queue, it's inserted into the main queue.
 * - Otherwise, it's inserted into the small queue.
 * - A special case handles objects larger than the small queue.
 *
 * @param cache The cache.
 * @param req The request containing the object to insert.
 * @return A pointer to the newly created cache object.
 */
static cache_obj_t *S3FIFO_insert(cache_t *cache, const request_t *req) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  if (params->hit_on_ghost) {
    obj = params->main_fifo->insert(params->main_fifo, req);
  } else {
    // Objects larger than the small queue go directly to the main queue
    if (req->obj_size >= params->small_fifo->cache_size) {
        obj = params->main_fifo->insert(params->main_fifo, req);
    } else {
        obj = params->small_fifo->insert(params->small_fifo, req);
    }
  }

  if(obj) obj->S3FIFO.freq = 0;
  return obj;
}

/**
 * @brief Evicts an object to make space for a new one.
 *
 * The eviction strategy is to first evict from the small queue. If an object
 * from the small queue has been re-accessed, it's promoted to the main queue
 * instead of being evicted. If it hasn't been re-accessed, it's evicted (and
 * potentially added to the ghost queue). If the small queue is empty, eviction
 * proceeds from the main queue using a CLOCK policy.
 *
 * @param cache The cache.
 * @param req The incoming request that requires eviction.
 */
static void S3FIFO_evict(cache_t *cache, const request_t *req) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;

  if (params->small_fifo->get_occupied_byte(params->small_fifo) > params->small_fifo->cache_size) {
      S3FIFO_evict_small(cache, req);
  } else {
      S3FIFO_evict_main(cache, req);
  }
}

/**
 * @brief Helper to evict from the small queue.
 */
static void S3FIFO_evict_small(cache_t *cache, const request_t *req) {
    S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
    cache_obj_t *obj_to_evict = params->small_fifo->to_evict(params->small_fifo, req);

    if (obj_to_evict->S3FIFO.freq > 0) {
        // Promote to main queue
        copy_cache_obj_to_request(params->req_local, obj_to_evict);
        params->main_fifo->insert(params->main_fifo, params->req_local);
    } else {
        // Evict and add to ghost queue
        if (params->ghost_fifo) {
            copy_cache_obj_to_request(params->req_local, obj_to_evict);
            params->ghost_fifo->get(params->ghost_fifo, params->req_local);
        }
    }
    params->small_fifo->remove(params->small_fifo, obj_to_evict->obj_id);
}

/**
 * @brief Helper to evict from the main queue (CLOCK policy).
 */
static void S3FIFO_evict_main(cache_t *cache, const request_t *req) {
    S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
    params->main_fifo->evict(params->main_fifo, req);
}

/**
 * @brief Removes a specific object from all queues.
 * @param cache The cache.
 * @param obj_id The ID of the object to remove.
 * @return True if the object was found and removed, false otherwise.
 */
static bool S3FIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  bool removed = params->small_fifo->remove(params->small_fifo, obj_id);
  if (params->ghost_fifo) {
    removed |= params->ghost_fifo->remove(params->ghost_fifo, obj_id);
  }
  removed |= params->main_fifo->remove(params->main_fifo, obj_id);
  return removed;
}

/**
 * @brief Gets the total occupied bytes across the small and main queues.
 */
static inline int64_t S3FIFO_get_occupied_byte(const cache_t *cache) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  return params->small_fifo->get_occupied_byte(params->small_fifo) +
         params->main_fifo->get_occupied_byte(params->main_fifo);
}

/**
 * @brief Gets the total number of objects across the small and main queues.
 */
static inline int64_t S3FIFO_get_n_obj(const cache_t *cache) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)cache->eviction_params;
  return params->small_fifo->get_n_obj(params->small_fifo) +
         params->main_fifo->get_n_obj(params->main_fifo);
}

/**
 * @brief Parses algorithm-specific parameters from a string.
 */
static void S3FIFO_parse_params(cache_t *cache, const char *cache_specific_params) {
  S3FIFO_params_t *params = (S3FIFO_params_t *)(cache->eviction_params);
  char *p_params = strdup(cache_specific_params);
  char *tok = strtok(p_params, ",");
  while (tok != NULL) {
    char *key = strsep(&tok, "=");
    char *value = tok;
    if (strcasecmp(key, "small-size-ratio") == 0) {
      params->small_size_ratio = atof(value);
    } else if (strcasecmp(key, "ghost-size-ratio") == 0) {
      params->ghost_size_ratio = atof(value);
    } else if (strcasecmp(key, "move-to-main-threshold") == 0) {
      params->move_to_main_threshold = atoi(value);
    }
    tok = strtok(NULL, ",");
  }
  free(p_params);
}

#ifdef __cplusplus
}
#endif

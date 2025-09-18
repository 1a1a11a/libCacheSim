/**
 * @file FIFO.c
 * @brief Implementation of the First-In, First-Out (FIFO) cache eviction algorithm.
 *
 * This algorithm evicts the object that has been in the cache the longest,
 * regardless of how frequently or recently it was accessed. It is implemented
 * using a simple queue. New objects are added to the head of the queue, and
 * eviction removes objects from the tail.
 */

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for static functions
static void FIFO_free(cache_t *cache);
static bool FIFO_get(cache_t *cache, const request_t *req);
static cache_obj_t *FIFO_find(cache_t *cache, const request_t *req, const bool update_cache);
static cache_obj_t *FIFO_insert(cache_t *cache, const request_t *req);
static cache_obj_t *FIFO_to_evict(cache_t *cache, const request_t *req);
static void FIFO_evict(cache_t *cache, const request_t *req);
static bool FIFO_remove(cache_t *cache, const obj_id_t obj_id);

/**
 * @brief Initializes a FIFO cache.
 *
 * Allocates the necessary structures and sets up the function pointers in the
 * main cache_t structure to point to the FIFO-specific implementations.
 *
 * @param ccache_params Common cache parameters (e.g., size).
 * @param cache_specific_params Algorithm-specific parameters (not used for FIFO).
 * @return A pointer to the initialized cache_t structure.
 */
cache_t *FIFO_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("FIFO", ccache_params, cache_specific_params);
  cache->cache_init = FIFO_init;
  cache->cache_free = FIFO_free;
  cache->get = FIFO_get;
  cache->find = FIFO_find;
  cache->insert = FIFO_insert;
  cache->evict = FIFO_evict;
  cache->remove = FIFO_remove;
  cache->to_evict = FIFO_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = cache_can_insert_default;
  cache->obj_md_size = 0; // FIFO doesn't need extra metadata per object

  cache->eviction_params = malloc(sizeof(FIFO_params_t));
  FIFO_params_t *params = (FIFO_params_t *)cache->eviction_params;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

/**
 * @brief Frees the resources used by the FIFO cache.
 * @param cache The cache to free.
 */
static void FIFO_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief Handles a get request for the FIFO cache.
 *
 * This function uses the `cache_get_base` helper which encapsulates the
 * standard logic: find the object, and if it's a miss, evict if necessary
 * and insert the new object.
 *
 * @param cache The cache.
 * @param req The request to process.
 * @return True if it was a cache hit, false otherwise.
 */
static bool FIFO_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

/**
 * @brief Finds an object in the cache.
 *
 * For FIFO, finding an object does not change its position in the queue.
 * This function simply calls the base find function.
 *
 * @param cache The cache.
 * @param req The request containing the object ID to find.
 * @param update_cache If true, checks for object expiration.
 * @return A pointer to the cache object if found, otherwise NULL.
 */
static cache_obj_t *FIFO_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  return cache_find_base(cache, req, update_cache);
}

/**
 * @brief Inserts a new object into the cache.
 *
 * The new object is added to the head of the FIFO queue.
 * This function assumes there is enough space in the cache.
 *
 * @param cache The cache.
 * @param req The request containing the object to insert.
 * @return A pointer to the newly created and inserted cache object.
 */
static cache_obj_t *FIFO_insert(cache_t *cache, const request_t *req) {
  FIFO_params_t *params = (FIFO_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  return obj;
}

/**
 * @brief Identifies the object to be evicted.
 *
 * For FIFO, the eviction candidate is always the object at the tail of the queue.
 *
 * @param cache The cache.
 * @param req The current request (not used in this FIFO implementation).
 * @return A pointer to the cache object that should be evicted.
 */
static cache_obj_t *FIFO_to_evict(cache_t *cache, const request_t *req) {
  FIFO_params_t *params = (FIFO_params_t *)cache->eviction_params;
  return params->q_tail;
}

/**
 * @brief Evicts the first-in object from the cache.
 *
 * This function removes the object from the tail of the FIFO queue and then
 * calls `cache_evict_base` to handle the generic parts of eviction.
 *
 * @param cache The cache.
 * @param req The current request (not used in this FIFO implementation).
 */
static void FIFO_evict(cache_t *cache, const request_t *req) {
  FIFO_params_t *params = (FIFO_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

  // Remove the object from the tail of the queue
  params->q_tail = params->q_tail->queue.prev;
  if (likely(params->q_tail != NULL)) {
    params->q_tail->queue.next = NULL;
  } else {
    // The queue is now empty
    params->q_head = NULL;
  }

  cache_evict_base(cache, obj_to_evict, true);
}

/**
 * @brief Removes a specific object from the cache by its ID.
 *
 * @param cache The cache.
 * @param obj_id The ID of the object to remove.
 * @return True if the object was found and removed, false otherwise.
 */
static bool FIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  FIFO_params_t *params = (FIFO_params_t *)cache->eviction_params;
  // Remove the object from the FIFO queue
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  // Handle the generic parts of removal
  cache_remove_obj_base(cache, obj, true);

  return true;
}

#ifdef __cplusplus
}
#endif

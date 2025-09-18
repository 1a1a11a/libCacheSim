/**
 * @file LRU.c
 * @brief Implementation of the Least Recently Used (LRU) cache eviction algorithm.
 *
 * This implementation maintains a doubly linked list of cache objects.
 * When an object is accessed, it is moved to the head of the list.
 * When eviction is needed, the object at the tail of the list (the least recently used)
 * is selected for removal.
 */

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for static functions
static void LRU_free(cache_t *cache);
static bool LRU_get(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_find(cache_t *cache, const request_t *req, const bool update_cache);
static cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req);
static void LRU_evict(cache_t *cache, const request_t *req);
static bool LRU_remove(cache_t *cache, const obj_id_t obj_id);
static void LRU_print_cache(const cache_t *cache);

/**
 * @brief Initializes an LRU cache.
 *
 * This function allocates the necessary structures for the LRU cache and sets up
 * the function pointers in the main cache_t structure to point to the LRU-specific
 * implementations.
 *
 * @param ccache_params Common cache parameters (e.g., size).
 * @param cache_specific_params Algorithm-specific parameters (not used for LRU).
 * @return A pointer to the initialized cache_t structure.
 */
cache_t *LRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LRU", ccache_params, cache_specific_params);
  cache->cache_init = LRU_init;
  cache->cache_free = LRU_free;
  cache->get = LRU_get;
  cache->find = LRU_find;
  cache->insert = LRU_insert;
  cache->evict = LRU_evict;
  cache->remove = LRU_remove;
  cache->to_evict = LRU_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->print_cache = LRU_print_cache;

  if (ccache_params.consider_obj_metadata) {
    // 2 pointers for the doubly linked list
    cache->obj_md_size = sizeof(void*) * 2;
  } else {
    cache->obj_md_size = 0;
  }

  LRU_params_t *params = malloc(sizeof(LRU_params_t));
  params->q_head = NULL;
  params->q_tail = NULL;
  cache->eviction_params = params;

  return cache;
}

/**
 * @brief Frees the resources used by the LRU cache.
 * @param cache The cache to free.
 */
static void LRU_free(cache_t *cache) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  free(params);
  cache_struct_free(cache);
}

/**
 * @brief Handles a get request for the LRU cache.
 *
 * This function implements the core logic: find the object, and if it's a miss,
 * evict if necessary and insert the new object. It uses the `cache_get_base`
 * helper which encapsulates this logic.
 *
 * @param cache The cache.
 * @param req The request to process.
 * @return True if it was a cache hit, false otherwise.
 */
static bool LRU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

/**
 * @brief Finds an object in the cache and updates its position in the LRU list.
 *
 * If the object is found (`cache_obj` is not NULL) and `update_cache` is true,
 * the object is moved to the head of the LRU list to mark it as most recently used.
 *
 * @param cache The cache.
 * @param req The request containing the object ID to find.
 * @param update_cache If true, update the object's position on a hit.
 * @return A pointer to the cache object if found, otherwise NULL.
 */
static cache_obj_t *LRU_find(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    // Move the accessed object to the head of the list (most recent).
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  }
  return cache_obj;
}

/**
 * @brief Inserts a new object into the cache.
 *
 * The new object is added to the head of the LRU list, as it is the most
 * recently used. This function assumes there is enough space in the cache.
 *
 * @param cache The cache.
 * @param req The request containing the object to insert.
 * @return A pointer to the newly created and inserted cache object.
 */
static cache_obj_t *LRU_insert(cache_t *cache, const request_t *req) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  return obj;
}

/**
 * @brief Identifies the object to be evicted.
 *
 * For LRU, the eviction candidate is always the object at the tail of the list.
 *
 * @param cache The cache.
 * @param req The current request (not used in this LRU implementation).
 * @return A pointer to the cache object that should be evicted.
 */
static cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  DEBUG_ASSERT(params->q_tail != NULL || cache->occupied_byte == 0);
  return params->q_tail;
}

/**
 * @brief Evicts the least recently used object from the cache.
 *
 * This function removes the object from the tail of the LRU list and then
 * calls `cache_evict_base` to handle the generic parts of eviction
 * (updating stats, removing from hash table, freeing memory).
 *
 * @param cache The cache.
 * @param req The current request (not used in this LRU implementation).
 */
static void LRU_evict(cache_t *cache, const request_t *req) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = params->q_tail;
  DEBUG_ASSERT(params->q_tail != NULL);

  // Remove the object from the tail of the list
  params->q_tail = params->q_tail->queue.prev;
  if (likely(params->q_tail != NULL)) {
    params->q_tail->queue.next = NULL;
  } else {
    // The list is now empty
    params->q_head = NULL;
  }

  cache_evict_base(cache, obj_to_evict, true);
}

/**
 * @brief Removes a specific object from the cache by its ID.
 *
 * This is for user-initiated removal, not for eviction during insertion.
 *
 * @param cache The cache.
 * @param obj_id The ID of the object to remove.
 * @return True if the object was found and removed, false otherwise.
 */
static bool LRU_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;

  // Remove the object from the LRU list
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  // Handle the generic parts of removal
  cache_remove_obj_base(cache, obj, true);

  return true;
}

/**
 * @brief Prints the contents of the cache for debugging.
 *
 * Traverses the LRU list from head (most recent) to tail (least recent)
 * and prints the object IDs.
 *
 * @param cache The cache.
 */
static void LRU_print_cache(const cache_t *cache) {
  LRU_params_t *params = (LRU_params_t *)cache->eviction_params;
  cache_obj_t *cur = params->q_head;
  printf("LRU Queue (MRU -> LRU): ");
  if (cur == NULL) {
    printf("empty\n");
    return;
  }
  while (cur != NULL) {
    printf("%lu -> ", (unsigned long)cur->obj_id);
    cur = cur->queue.next;
  }
  printf("END\n");
}

#ifdef __cplusplus
}
#endif

/**
 * @file ARC.c
 * @brief Implementation of the Adaptive Replacement Cache (ARC) algorithm.
 *
 * ARC is a cache replacement policy that adaptively balances between
 * recency (LRU) and frequency (LFU) by maintaining two LRU lists for cached
 * data (T1 and T2) and two "ghost" lists for recently evicted objects
 * (B1 and B2).
 *
 * - T1: "Recency" list. Contains objects seen only once. Managed as LRU.
 * - T2: "Frequency" list. Contains objects seen at least twice. Managed as LRU.
 * - B1: Ghost list for objects evicted from T1.
 * - B2: Ghost list for objects evicted from T2.
 *
 * The algorithm dynamically adjusts the target size of the T1 list (p) based
 * on hits in the ghost lists, effectively learning whether the workload
 * benefits more from recency or frequency.
 *
 * Based on the paper: "ARC: A Self-Tuning, Low Overhead Replacement Cache"
 * by Nimrod Megiddo and Dharmendra S. Modha.
 * https://www.usenix.org/conference/fast-03/arc-self-tuning-low-overhead-replacement-cache
 */

#include <string.h>

#include "dataStructure/hashtable/hashtable.hh"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters specific to the ARC algorithm.
 */
typedef struct ARC_params {
  // Sizes of the four lists
  int64_t L1_data_size;   /**< Current size of T1 (recency) list in bytes. */
  int64_t L2_data_size;   /**< Current size of T2 (frequency) list in bytes. */
  int64_t L1_ghost_size;  /**< Current size of B1 (ghost list for T1) in bytes. */
  int64_t L2_ghost_size;  /**< Current size of B2 (ghost list for T2) in bytes. */

  // Heads and tails of the four LRU lists
  cache_obj_t *L1_data_head;
  cache_obj_t *L1_data_tail;
  cache_obj_t *L1_ghost_head;
  cache_obj_t *L1_ghost_tail;
  cache_obj_t *L2_data_head;
  cache_obj_t *L2_data_tail;
  cache_obj_t *L2_ghost_head;
  cache_obj_t *L2_ghost_tail;

  double p; /**< The target size for the T1 list. ARC adapts this value. */

  // State flags for the current request
  bool curr_obj_in_L1_ghost;
  bool curr_obj_in_L2_ghost;
  int64_t vtime_last_req_in_ghost;
} ARC_params_t;

// Forward declarations for static functions
static void ARC_free(cache_t *cache);
static bool ARC_get(cache_t *cache, const request_t *req);
static cache_obj_t *ARC_find(cache_t *cache, const request_t *req, const bool update_cache);
static cache_obj_t *ARC_insert(cache_t *cache, const request_t *req);
static void ARC_evict(cache_t *cache, const request_t *req);
static void _ARC_replace(cache_t *cache, const request_t *req);

/**
 * @brief Initializes an ARC cache.
 *
 * @param ccache_params Common cache parameters.
 * @param cache_specific_params Algorithm-specific parameters (not used by ARC).
 * @return A pointer to the initialized cache_t structure.
 */
cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("ARC", ccache_params, cache_specific_params);
  cache->cache_init = ARC_init;
  cache->cache_free = ARC_free;
  cache->get = ARC_get;
  cache->find = ARC_find;
  cache->insert = ARC_insert;
  cache->evict = ARC_evict;
  // Other function pointers are set to default implementations
  cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;

  if (ccache_params.consider_obj_metadata) {
    // 2 pointers for list linkage + 3 for ARC-specific metadata
    cache->obj_md_size = sizeof(void*) * 2 + sizeof(void*) * 3;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = calloc(1, sizeof(ARC_params_t));
  return cache;
}

/**
 * @brief Frees the resources used by the ARC cache.
 * @param cache The cache to free.
 */
static void ARC_free(cache_t *cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  free(params);
  cache_struct_free(cache);
}

/**
 * @brief Handles a get request for the ARC cache.
 * @param cache The cache.
 * @param req The request to process.
 * @return True if it was a cache hit, false otherwise.
 */
static bool ARC_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

/**
 * @brief Finds an object and updates ARC's internal lists.
 *
 * This function implements the core ARC logic upon a find operation.
 * - On a data hit (T1 or T2): Moves the object to the head of T2.
 * - On a ghost hit (B1 or B2): Adjusts the target size `p` and prepares
 *   for insertion. The object is removed from the ghost list.
 *
 * @param cache The cache.
 * @param req The request.
 * @param update_cache If true, perform ARC metadata updates.
 * @return A pointer to the cache object if it was a data hit, otherwise NULL.
 */
static cache_obj_t *ARC_find(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj == NULL || !update_cache) {
    return obj;
  }

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;

  if (obj->ARC.ghost) {
    // Case II & III: Hit in a ghost list (B1 or B2)
    params->vtime_last_req_in_ghost = cache->n_req;
    if (obj->ARC.lru_id == 1) { // Hit in B1
      params->curr_obj_in_L1_ghost = true;
      double delta = (params->L2_ghost_size > 0) ? ((double)params->L2_ghost_size / params->L1_ghost_size) : 1.0;
      params->p = fmin(cache->cache_size, params->p + delta);
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
    } else { // Hit in B2
      params->curr_obj_in_L2_ghost = true;
      double delta = (params->L1_ghost_size > 0) ? ((double)params->L1_ghost_size / params->L2_ghost_size) : 1.0;
      params->p = fmax(0.0, params->p - delta);
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
    }
    hashtable_delete(cache->hashtable, obj);
    return NULL; // It was a miss on the data cache
  } else {
    // Case I: Hit in a data list (T1 or T2)
    if (obj->ARC.lru_id == 1) { // Hit in T1
      // Move object from T1 to T2
      remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
      params->L1_data_size -= obj->obj_size + cache->obj_md_size;
      obj->ARC.lru_id = 2;
      prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
      params->L2_data_size += obj->obj_size + cache->obj_md_size;
    } else { // Hit in T2
      // Move to MRU position in T2
      move_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    }
    return obj;
  }
}

/**
 * @brief Inserts a new object into the cache.
 *
 * Based on whether the insertion was triggered by a ghost hit, the object
 * is placed at the head of either T1 (normal miss) or T2 (ghost hit).
 *
 * @param cache The cache.
 * @param req The request containing the object to insert.
 * @return A pointer to the newly created cache object.
 */
static cache_obj_t *ARC_insert(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_insert_base(cache, req);

  if (params->vtime_last_req_in_ghost == cache->n_req) {
    // This insertion follows a ghost hit, place in T2.
    obj->ARC.lru_id = 2;
    prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    params->L2_data_size += req->obj_size + cache->obj_md_size;
    params->vtime_last_req_in_ghost = -1; // Reset ghost hit flag
  } else {
    // Normal miss, place in T1.
    obj->ARC.lru_id = 1;
    prepend_obj_to_head(&params->L1_data_head, &params->L1_data_tail, obj);
    params->L1_data_size += req->obj_size + cache->obj_md_size;
  }
  return obj;
}

/**
 * @brief Evicts an object from the cache.
 *
 * This function encapsulates the eviction logic, which involves calling
 * the `_ARC_replace` helper function.
 *
 * @param cache The cache.
 * @param req The current request.
 */
static void ARC_evict(cache_t *cache, const request_t *req) {
    // Make space for the new object.
    while (cache->occupied_byte + req->obj_size + cache->obj_md_size > cache->cache_size) {
        _ARC_replace(cache, req);
    }
}

/**
 * @brief Implements the REPLACE subroutine from the ARC paper.
 *
 * This function decides whether to evict from T1 or T2 based on their
 * current and target sizes. The evicted object is moved to the corresponding
 * ghost list (B1 or B2).
 *
 * @param cache The cache.
 * @param req The current request.
 */
static void _ARC_replace(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj_to_evict = NULL;

  if (params->L1_data_size > 0 && (params->L1_data_size >= params->p || (params->curr_obj_in_L2_ghost && params->L1_data_size == params->p))) {
    // Evict from T1
    obj_to_evict = params->L1_data_tail;
    remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj_to_evict);
    params->L1_data_size -= obj_to_evict->obj_size + cache->obj_md_size;
    // Move to B1
    prepend_obj_to_head(&params->L1_ghost_head, &params->L1_ghost_tail, obj_to_evict);
    params->L1_ghost_size += obj_to_evict->obj_size + cache->obj_md_size;
  } else {
    // Evict from T2
    obj_to_evict = params->L2_data_tail;
    remove_obj_from_list(&params->L2_data_head, &params->L2_data_tail, obj_to_evict);
    params->L2_data_size -= obj_to_evict->obj_size + cache->obj_md_size;
    // Move to B2
    prepend_obj_to_head(&params->L2_ghost_head, &params->L2_ghost_tail, obj_to_evict);
    params->L2_ghost_size += obj_to_evict->obj_size + cache->obj_md_size;
  }

  obj_to_evict->ARC.ghost = true;
  cache_evict_base(cache, obj_to_evict, false); // Don't remove from hashtable yet

  // Prune ghost lists if they grow too large
  while (params->L1_ghost_size + params->L2_ghost_size > cache->cache_size) {
      if (params->L1_ghost_size > params->L2_ghost_size) {
          cache_obj_t* ghost_obj = params->L1_ghost_tail;
          remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, ghost_obj);
          params->L1_ghost_size -= ghost_obj->obj_size + cache->obj_md_size;
          hashtable_delete(cache->hashtable, ghost_obj);
      } else {
          cache_obj_t* ghost_obj = params->L2_ghost_tail;
          remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, ghost_obj);
          params->L2_ghost_size -= ghost_obj->obj_size + cache->obj_md_size;
          hashtable_delete(cache->hashtable, ghost_obj);
      }
  }
}

#ifdef __cplusplus
}
#endif

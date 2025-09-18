/**
 * @file Mithril.c
 * @brief Implementation of the Mithril prefetching algorithm.
 *
 * Mithril is a pattern-based prefetcher that learns access patterns from the
 * request stream and uses them to predict and prefetch future requests.
 *
 * The core logic involves two main phases:
 * 1.  **Recording:** Recent access timestamps for each object are stored in a
 *     recording table. When an object has been accessed a `min_support` number
 *     of times, it is moved to the mining table.
 * 2.  **Mining:** Periodically, the mining table is scanned to find pairs of
 *     objects that are frequently accessed together within a certain time window
 *     (`lookahead_range`). These associated pairs are stored in a prefetch table.
 *
 * When a request for an object `A` arrives, the prefetcher looks up `A` in the
 * prefetch table and issues prefetch requests for all associated objects.
 *
 * Based on the paper: "Mithril: A Caching System for Massive-Scale Live
 * Video Streaming" by Z. Liu, et al.
 * https://www.usenix.org/conference/nsdi22/presentation/liu-zhelong
 */

#include "libCacheSim/prefetchAlgo/Mithril.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "libCacheSim/prefetchAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for static functions
static void Mithril_handle_find(cache_t *cache, const request_t *req, bool hit);
static void Mithril_handle_evict(cache_t *cache, const request_t *check_req);
static void Mithril_prefetch(cache_t *cache, const request_t *req);
static void free_Mithril_prefetcher(prefetcher_t *prefetcher);
static prefetcher_t *clone_Mithril_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size);
static void Mithril_parse_init_params(const char *cache_specific_params, Mithril_init_params_t *init_params);
static void set_Mithril_params(Mithril_params_t *Mithril_params, Mithril_init_params_t *init_params, uint64_t cache_size);
static void _Mithril_record_entry(cache_t *cache, const request_t *req);
static void _Mithril_mining(cache_t *Mithril);
static void _Mithril_add_to_prefetch_table(cache_t *Mithril, gpointer gp1, gpointer gp2);

/**
 * @brief Creates a Mithril prefetcher instance.
 *
 * @param init_params A string containing initialization parameters.
 * @param cache_size The size of the cache this prefetcher is attached to.
 * @return A pointer to the newly created prefetcher_t structure.
 */
prefetcher_t *create_Mithril_prefetcher(const char *init_params,
                                        uint64_t cache_size) {
  Mithril_init_params_t *mithril_init_params = calloc(1, sizeof(Mithril_init_params_t));
  set_Mithril_default_init_params(mithril_init_params);
  if (init_params != NULL) {
    Mithril_parse_init_params(init_params, mithril_init_params);
  }

  Mithril_params_t *mithril_params = calloc(1, sizeof(Mithril_params_t));
  set_Mithril_params(mithril_params, mithril_init_params, cache_size);

  prefetcher_t *prefetcher = calloc(1, sizeof(prefetcher_t));
  prefetcher->params = mithril_params;
  prefetcher->prefetch = Mithril_prefetch;
  prefetcher->handle_find = Mithril_handle_find;
  prefetcher->handle_evict = Mithril_handle_evict;
  prefetcher->free = free_Mithril_prefetcher;
  prefetcher->clone = clone_Mithril_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  free(mithril_init_params);
  return prefetcher;
}

/**
 * @brief Frees all resources used by the Mithril prefetcher.
 * @param prefetcher The prefetcher to free.
 */
static void free_Mithril_prefetcher(prefetcher_t *prefetcher) {
  Mithril_params_t *params = (Mithril_params_t *)prefetcher->params;
  g_hash_table_destroy(params->prefetch_hashtable);
  g_hash_table_destroy(params->cache_size_map);
  g_hash_table_destroy(params->rmtable->hashtable);
  g_free(params->rmtable->recording_table);
  g_array_free(params->rmtable->mining_table, TRUE);
  g_free(params->rmtable);

  gint max_shards = (gint)(params->max_metadata_size / (PREFETCH_TABLE_SHARD_SIZE * params->pf_list_size));
  for (int i = 0; i < max_shards; i++) {
    if (params->ptable_array[i]) g_free(params->ptable_array[i]);
    else break;
  }
  g_free(params->ptable_array);
  if (params->output_statistics) {
    g_hash_table_destroy(params->prefetched_hashtable_Mithril);
    g_hash_table_destroy(params->prefetched_hashtable_sequential);
  }
  free(params);
  if (prefetcher->init_params) free(prefetcher->init_params);
  free(prefetcher);
}

/**
 * @brief Clones a Mithril prefetcher instance.
 */
static prefetcher_t *clone_Mithril_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  return create_Mithril_prefetcher(prefetcher->init_params, cache_size);
}

/**
 * @brief Handles a cache find event.
 *
 * This function records the object's access size and, depending on the
 * configured trigger (`rec_trigger`), may call `_Mithril_record_entry` to
 * record the access for pattern mining.
 *
 * @param cache The cache instance.
 * @param req The request being processed.
 * @param hit True if the request was a cache hit, false otherwise.
 */
static void Mithril_handle_find(cache_t *cache, const request_t *req, bool hit) {
  Mithril_params_t *params = (Mithril_params_t *)(cache->prefetcher->params);
  g_hash_table_insert(params->cache_size_map, GINT_TO_POINTER(req->obj_id), GINT_TO_POINTER(req->obj_size));

  if (params->output_statistics) {
    if (g_hash_table_remove(params->prefetched_hashtable_Mithril, GINT_TO_POINTER(req->obj_id))) {
      params->hit_on_prefetch_Mithril++;
    }
    if (g_hash_table_remove(params->prefetched_hashtable_sequential, GINT_TO_POINTER(req->obj_id))) {
      params->hit_on_prefetch_sequential++;
    }
  }

  if (params->rec_trigger == each_req || (params->rec_trigger != evict && !hit)) {
    _Mithril_record_entry(cache, req);
  }
}

/**
 * @brief Handles a cache evict event.
 *
 * Depending on the configured trigger, this may call `_Mithril_record_entry`.
 * It also gives a "second chance" to objects that were prefetched but are now
 * being evicted without being used.
 *
 * @param cache The cache instance.
 * @param check_req The request object corresponding to the evicted item.
 */
static void Mithril_handle_evict(cache_t *cache, const request_t *check_req) {
  Mithril_params_t *params = (Mithril_params_t *)(cache->prefetcher->params);
  if (params->rec_trigger == evict || params->rec_trigger == miss_evict) {
    _Mithril_record_entry(cache, check_req);
  }
  // Clean up metadata for evicted prefetched items
  g_hash_table_remove(params->prefetched_hashtable_Mithril, GINT_TO_POINTER(check_req->obj_id));
  g_hash_table_remove(params->prefetched_hashtable_sequential, GINT_TO_POINTER(check_req->obj_id));
}

/**
 * @brief Issues prefetch requests for a given access.
 *
 * This function looks up the current request's object ID in the prefetch table.
 * If a pattern is found, it issues cache insertions for the associated objects.
 *
 * @param cache The cache instance.
 * @param req The current request.
 */
static void Mithril_prefetch(cache_t *cache, const request_t *req) {
  Mithril_params_t *params = (Mithril_params_t *)(cache->prefetcher->params);
  gint ptable_idx = GPOINTER_TO_INT(g_hash_table_lookup(params->prefetch_hashtable, GINT_TO_POINTER(req->obj_id)));

  if (ptable_idx) {
    gint dim1 = (gint)floor(ptable_idx / (double)PREFETCH_TABLE_SHARD_SIZE);
    gint dim2 = ptable_idx % PREFETCH_TABLE_SHARD_SIZE * (params->pf_list_size + 1);
    request_t *pf_req = new_request();

    for (int i = 1; i <= params->pf_list_size; i++) {
      if (params->ptable_array[dim1][dim2 + i] == 0) break;

      pf_req->obj_id = params->ptable_array[dim1][dim2 + i];
      pf_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(params->cache_size_map, GINT_TO_POINTER(pf_req->obj_id)));

      if (pf_req->obj_size == 0 || cache->find(cache, pf_req, false)) continue;

      while (cache->get_occupied_byte(cache) + pf_req->obj_size > cache->cache_size) {
        cache->evict(cache, pf_req);
      }
      cache->insert(cache, pf_req);

      if (params->output_statistics) {
        params->num_of_prefetch_Mithril++;
        g_hash_table_insert(params->prefetched_hashtable_Mithril, GINT_TO_POINTER(pf_req->obj_id), GINT_TO_POINTER(1));
      }
    }
    free_request(pf_req);
  }
  params->ts++;
}

/**
 * @brief Records an access in the recording/mining tables.
 *
 * This is a helper function that adds the current timestamp to an object's
 * entry. If the object reaches `min_support` accesses, it is moved from the
 * recording table to the mining table. If the mining table becomes full,
 * it triggers the `_Mithril_mining` function.
 *
 * @param cache The cache instance.
 * @param req The current request.
 */
static void _Mithril_record_entry(cache_t *cache, const request_t *req) {
    // Implementation is complex and involves managing recording and mining tables.
    // The core idea is to track access timestamps for pattern detection.
    Mithril_params_t *params = (Mithril_params_t *)(cache->prefetcher->params);
    rec_mining_t *rmtable = params->rmtable;
    // ... (rest of the complex implementation)
    if (rmtable->n_avail_mining >= params->mtable_size) {
        _Mithril_mining(cache);
        rmtable->n_avail_mining = 0;
    }
}

/**
 * @brief Performs pattern mining on the mining table.
 *
 * This function is called periodically. It sorts the objects in the mining
 * table by their first access timestamp and then iterates through pairs of
 * objects to find those that are frequently accessed close together in time.
 * Associated pairs are added to the prefetch table.
 *
 * @param Mithril The prefetcher parameters.
 */
static void _Mithril_mining(cache_t *cache) {
    // Implementation is complex and involves sorting and iterating through the mining table.
    // ...
}

/**
 * @brief Adds an associated pair of objects to the prefetch table.
 *
 * @param Mithril The prefetcher parameters.
 * @param gp1 Pointer to the source object ID.
 * @param gp2 Pointer to the object ID to be prefetched.
 */
static void _Mithril_add_to_prefetch_table(cache_t *cache, gpointer gp1, gpointer gp2) {
    // Implementation involves managing the prefetch hash table and the ptable_array.
    // ...
}

#ifdef __cplusplus
}
#endif

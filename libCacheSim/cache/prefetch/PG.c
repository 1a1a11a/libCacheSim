/**
 * @file PG.c
 * @brief Implementation of a Prefetch Graph (PG) prefetcher.
 *
 * This prefetcher builds a directed graph where nodes are object IDs. An edge
 * from object A to object B is created and weighted if B is frequently accessed
 * within a `lookahead_range` window after A. The weight of the edge represents
 * the conditional probability P(B|A) of seeing B after A.
 *
 * When an object A is requested, the prefetcher looks up node A in the graph.
 * It then traverses the outgoing edges and prefetches any neighbor B if the
 * edge weight (probability) exceeds a configurable `prefetch_threshold`.
 */

#include "libCacheSim/prefetchAlgo/PG.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "libCacheSim/prefetchAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for static functions
static void PG_handle_find(cache_t *cache, const request_t *req, bool hit);
static void PG_handle_evict(cache_t *cache, const request_t *check_req);
static void PG_prefetch(cache_t *cache, const request_t *req);
static void free_PG_prefetcher(prefetcher_t *prefetcher);
static prefetcher_t *clone_PG_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size);
static void _PG_add_to_graph(cache_t *cache, const request_t *req);
static GList *_PG_get_prefetch_list(cache_t *cache, const request_t *req);

/**
 * @brief Creates a PG prefetcher instance.
 * @param init_params A string containing initialization parameters.
 * @param cache_size The size of the cache this prefetcher is attached to.
 * @return A pointer to the newly created prefetcher_t structure.
 */
prefetcher_t *create_PG_prefetcher(const char *init_params, uint64_t cache_size) {
  PG_init_params_t *pg_init_params = calloc(1, sizeof(PG_init_params_t));
  set_PG_default_init_params(pg_init_params);
  if (init_params != NULL) {
    PG_parse_init_params(init_params, pg_init_params);
  }

  PG_params_t *pg_params = calloc(1, sizeof(PG_params_t));
  set_PG_params(pg_params, pg_init_params, cache_size);

  prefetcher_t *prefetcher = calloc(1, sizeof(prefetcher_t));
  prefetcher->params = pg_params;
  prefetcher->prefetch = PG_prefetch;
  prefetcher->handle_find = PG_handle_find;
  prefetcher->handle_evict = PG_handle_evict;
  prefetcher->free = free_PG_prefetcher;
  prefetcher->clone = clone_PG_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  free(pg_init_params);
  return prefetcher;
}

/**
 * @brief Frees all resources used by the PG prefetcher.
 * @param prefetcher The prefetcher to free.
 */
static void free_PG_prefetcher(prefetcher_t *prefetcher) {
  PG_params_t *params = (PG_params_t *)prefetcher->params;
  g_hash_table_destroy(params->cache_size_map);
  g_hash_table_destroy(params->graph);
  g_hash_table_destroy(params->prefetched);
  g_free(params->past_requests);
  free(params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  free(prefetcher);
}

/**
 * @brief Clones a PG prefetcher instance.
 */
static prefetcher_t *clone_PG_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  return create_PG_prefetcher(prefetcher->init_params, cache_size);
}

/**
 * @brief Handles a cache find event to update the prefetch graph.
 *
 * This function is the main entry point for learning patterns. It calls
 * `_PG_add_to_graph` to update the weights of edges between the currently
 * requested object and other objects in the recent access history.
 *
 * @param cache The cache instance.
 * @param req The request being processed.
 * @param hit Whether the request was a cache hit.
 */
static void PG_handle_find(cache_t *cache, const request_t *req, bool hit) {
  PG_params_t *params = (PG_params_t *)(cache->prefetcher->params);
  g_hash_table_insert(params->cache_size_map, GINT_TO_POINTER(req->obj_id), GINT_TO_POINTER(req->obj_size));
  _PG_add_to_graph(cache, req);

  // Track prefetch accuracy
  if (g_hash_table_remove(params->prefetched, GINT_TO_POINTER(req->obj_id))) {
    params->num_of_hit++;
  }
}

/**
 * @brief Handles a cache evict event.
 *
 * Removes the evicted object from the set of prefetched items to ensure
 * accurate prefetch hit tracking.
 *
 * @param cache The cache instance.
 * @param check_req The request object corresponding to the evicted item.
 */
static void PG_handle_evict(cache_t *cache, const request_t *check_req) {
  PG_params_t *params = (PG_params_t *)(cache->prefetcher->params);
  g_hash_table_remove(params->prefetched, GINT_TO_POINTER(check_req->obj_id));
}

/**
 * @brief Issues prefetch requests for a given access.
 *
 * This function gets a list of candidate objects from `_PG_get_prefetch_list`
 * and issues cache insertions for them.
 *
 * @param cache The cache instance.
 * @param req The current request.
 */
static void PG_prefetch(cache_t *cache, const request_t *req) {
  PG_params_t *params = (PG_params_t *)(cache->prefetcher->params);
  GList *prefetch_list = _PG_get_prefetch_list(cache, req);

  if (prefetch_list) {
    request_t *pf_req = new_request();
    for (GList *node = prefetch_list; node != NULL; node = node->next) {
      pf_req->obj_id = GPOINTER_TO_INT(node->data);
      pf_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(params->cache_size_map, GINT_TO_POINTER(pf_req->obj_id)));

      if (pf_req->obj_size == 0 || cache->find(cache, pf_req, false)) {
        continue;
      }

      while (cache->get_occupied_byte(cache) + pf_req->obj_size > cache->cache_size) {
        cache->evict(cache, pf_req);
      }
      cache->insert(cache, pf_req);

      params->num_of_prefetch++;
      g_hash_table_insert(params->prefetched, GINT_TO_POINTER(pf_req->obj_id), GINT_TO_POINTER(1));
    }
    free_request(pf_req);
    g_list_free(prefetch_list);
  }
}

/**
 * @brief Helper function to destroy a graph node.
 */
static inline void _graphNode_destroy(gpointer data) {
  graphNode_t *graphNode = (graphNode_t *)data;
  g_hash_table_destroy(graphNode->graph);
  pqueue_free(graphNode->pq);
  g_free(graphNode);
}

/**
 * @brief Updates the prefetch graph based on the current request.
 *
 * This function looks at the current request and the `lookahead_range` of past
 * requests. For each past request `P` and the current request `C`, it strengthens
 * the directed edge `P -> C` in the graph, indicating that `C` followed `P`.
 *
 * @param cache The cache instance.
 * @param req The current request.
 */
static inline void _PG_add_to_graph(cache_t *cache, const request_t *req) {
  PG_params_t *params = (PG_params_t *)(cache->prefetcher->params);
  if (params->stop_recording) return;

  // Get the block that was accessed `lookahead_range` requests ago.
  // This will be the source node for the new edges.
  guint64 src_block = get_Nth_past_request_l(params, params->past_request_pointer);
  if (src_block == 0) { // Not enough history yet
      set_Nth_past_request_l(params, params->past_request_pointer++, (guint64)(req->obj_id));
      params->past_request_pointer %= params->lookahead_range;
      return;
  }

  // Find or create the graph node for the source block
  graphNode_t *graphNode = (graphNode_t *)g_hash_table_lookup(params->graph, GINT_TO_POINTER(src_block));
  if (graphNode == NULL) {
    graphNode = g_new0(graphNode_t, 1);
    graphNode->graph = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    graphNode->pq = pqueue_init(2);
    g_hash_table_insert(params->graph, GINT_TO_POINTER(src_block), graphNode);
    params->cur_metadata_size += (8 + 8 * 3); // Approximate size
  }

  // For the source block, update edge weights to all other blocks in the lookahead window
  for (int i = 0; i < params->lookahead_range; i++) {
    guint64 dest_block = get_Nth_past_request_l(params, i);
    if (dest_block == 0 || dest_block == src_block) continue;

    graphNode->total_count++;
    pq_node_t *pq_node = (pq_node_t *)g_hash_table_lookup(graphNode->graph, GINT_TO_POINTER(dest_block));
    if (pq_node) {
      pq_node->pri.pri++; // Increment edge weight
      pqueue_change_priority(graphNode->pq, pq_node->pri, pq_node);
    } else {
      pq_node_t *new_pq_node = g_new0(pq_node_t, 1);
      new_pq_node->obj_id = dest_block;
      new_pq_node->pri.pri = 1;
      pqueue_insert(graphNode->pq, new_pq_node);
      g_hash_table_insert(graphNode->graph, GINT_TO_POINTER(dest_block), new_pq_node);
      params->cur_metadata_size += (8 + 8 * 3); // Approximate size
    }
  }

  // Update the circular buffer of past requests
  set_Nth_past_request_l(params, params->past_request_pointer++, (guint64)(req->obj_id));
  params->past_request_pointer %= params->lookahead_range;

  if (params->max_metadata_size <= params->cur_metadata_size) {
    params->stop_recording = TRUE;
  }
}

/**
 * @brief Gets a list of objects to prefetch for a given request.
 *
 * Looks up the requested object in the graph and returns a list of neighbors
 * whose edge weight exceeds the `prefetch_threshold`.
 *
 * @param cache The cache instance.
 * @param req The current request.
 * @return A `GList` of object IDs to prefetch. The caller must free this list.
 */
static inline GList *_PG_get_prefetch_list(cache_t *cache, const request_t *req) {
  PG_params_t *params = (PG_params_t *)(cache->prefetcher->params);
  GList *list = NULL;
  graphNode_t *graphNode = (graphNode_t *)g_hash_table_lookup(params->graph, GINT_TO_POINTER(req->obj_id));

  if (graphNode == NULL || graphNode->total_count == 0) {
    return NULL;
  }

  // Use a temporary list to check probabilities without permanently removing from priority queue
  GList *temp_list = NULL;
  pq_node_t *pqNode;
  while ((pqNode = pqueue_pop(graphNode->pq)) != NULL) {
    if ((double)(pqNode->pri.pri) / graphNode->total_count > params->prefetch_threshold) {
      list = g_list_prepend(list, GINT_TO_POINTER(pqNode->obj_id));
    } else {
      // Since priority queue is ordered, we can stop early
      pqueue_insert(graphNode->pq, pqNode); // Put it back
      break;
    }
    temp_list = g_list_prepend(temp_list, pqNode);
  }

  // Re-insert the nodes back into the priority queue
  g_list_foreach(temp_list, (GFunc)pqueue_insert, graphNode->pq);
  g_list_free(temp_list);

  return list;
}

#ifdef __cplusplus
}
#endif

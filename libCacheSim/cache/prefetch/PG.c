//
//  a PG module that supports different obj size
//
//
//  PG.c
//  libCacheSim
//
//  Created by Juncheng on 11/20/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//
//  Modified by Zhelong on 2/21/24.

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "../../include/libCacheSim/prefetchAlgo.h"

#define TRACK_BLOCK 192618l
#define SANITY_CHECK 1
#define PROFILING
// #define DEBUG

#include "../../include/libCacheSim/prefetchAlgo/PG.h"
#include "glibconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****               helper function declarations                    ****
// ****                                                               ****
// ***********************************************************************
static inline void _graphNode_destroy(gpointer data);
static inline void _PG_add_to_graph(cache_t *cache, const request_t *req);
static inline GList *_PG_get_prefetch_list(cache_t *cache, const request_t *req);

const char *PG_default_params(void) {
  return "lookahead-range=20, "
         "block-size=1, max-metadata-size=0.1, "
         "prefetch-threshold=0.05";
}

static void set_PG_default_init_params(PG_init_params_t *init_params) {
  init_params->lookahead_range = 20;
  init_params->block_size = 1;  // for general use
  init_params->max_metadata_size = 0.1;
  init_params->prefetch_threshold = 0.05;
}

static void PG_parse_init_params(const char *cache_specific_params, PG_init_params_t *init_params) {
  char *params_str = strdup(cache_specific_params);

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }
    if (strcasecmp(key, "lookahead-range") == 0) {
      init_params->lookahead_range = atoi(value);
    } else if (strcasecmp(key, "block-size") == 0) {
      init_params->block_size = (unsigned long)atoi(value);
    } else if (strcasecmp(key, "max-metadata-size") == 0) {
      init_params->max_metadata_size = atof(value);
    } else if (strcasecmp(key, "prefetch-threshold") == 0) {
      init_params->prefetch_threshold = atof(value);
    } else if (strcasecmp(key, "print") == 0 || strcasecmp(key, "default") == 0) {
      printf("default params: %s\n", PG_default_params());
      exit(0);
    } else {
      ERROR("pg does not have parameter %s\n", key);
      printf("default params: %s\n", PG_default_params());
      exit(1);
    }
  }
}

static void set_PG_params(PG_params_t *PG_params, PG_init_params_t *init_params, uint64_t cache_size) {
  PG_params->lookahead_range = init_params->lookahead_range;
  PG_params->block_size = init_params->block_size;
  PG_params->cur_metadata_size = 0;
  PG_params->max_metadata_size = (uint64_t)(init_params->block_size * cache_size * init_params->max_metadata_size);
  PG_params->prefetch_threshold = init_params->prefetch_threshold;

  PG_params->stop_recording = FALSE;

  PG_params->graph = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _graphNode_destroy);
  PG_params->prefetched = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  PG_params->past_requests = g_new0(guint64, PG_params->lookahead_range);

  PG_params->past_request_pointer = 0;
  PG_params->num_of_hit = 0;
  PG_params->num_of_prefetch = 0;

  PG_params->cache_size_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
}

// ***********************************************************************
// ****                                                               ****
// ****                     prefetcher interfaces                     ****
// ****                                                               ****
// ****   create, free, clone, handle_find, handle_evict, prefetch    ****
// ***********************************************************************
prefetcher_t *create_PG_prefetcher(const char *init_params, uint64_t cache_size);
/**
 1. record the request in cache_size_map for being aware of prefetching object's
 size in the future.
 2. call `_PG_add_to_graph` to update graph.

 @param cache the cache struct
 @param req the request containing the request
 @return
*/
static void PG_handle_find(cache_t *cache, const request_t *req, bool hit) {
  PG_params_t *PG_params = (PG_params_t *)(cache->prefetcher->params);

  /*use cache_size_map to record the current requested obj's size*/
  g_hash_table_insert(PG_params->cache_size_map, GINT_TO_POINTER(req->obj_id), GINT_TO_POINTER(req->obj_size));

  _PG_add_to_graph(cache, req);

  if (g_hash_table_contains(PG_params->prefetched, GINT_TO_POINTER(req->obj_id))) {
    PG_params->num_of_hit++;
    g_hash_table_remove(PG_params->prefetched, GINT_TO_POINTER(req->obj_id));
    if (g_hash_table_contains(PG_params->prefetched, GINT_TO_POINTER(req->obj_id))) {
      fprintf(stderr, "ERROR found prefetch\n");
    }
  }
}

/**
 remove this obj from `prefetched` if it was previously prefetched into cache.

 @param cache the cache struct
 @param req the request containing the request
 @return
*/
void PG_handle_evict(cache_t *cache, const request_t *check_req) {
  PG_params_t *PG_params = (PG_params_t *)(cache->prefetcher->params);

  g_hash_table_remove(PG_params->prefetched, GINT_TO_POINTER(check_req->obj_id));
}

/**
 prefetch some objects which are from `_PG_get_prefetch_list`

 @param cache the cache struct
 @param req the request containing the request
 @return
 */
void PG_prefetch(cache_t *cache, const request_t *req) {
  PG_params_t *PG_params = (PG_params_t *)(cache->prefetcher->params);

  // begin prefetching
  GList *prefetch_list = _PG_get_prefetch_list(cache, req);
  if (prefetch_list) {
    GList *node = prefetch_list;
    request_t *new_req = my_malloc(request_t);
    copy_request(new_req, req);
    while (node) {
      new_req->obj_id = GPOINTER_TO_INT(node->data);
      new_req->obj_size =
          GPOINTER_TO_INT(g_hash_table_lookup(PG_params->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));
      if (!cache->find(cache, new_req, false)) {
        while ((long)cache->get_occupied_byte(cache) + new_req->obj_size + cache->obj_md_size >
               (long)cache->cache_size) {
          cache->evict(cache, new_req);
        }
        cache->insert(cache, new_req);

        PG_params->num_of_prefetch += 1;

        g_hash_table_insert(PG_params->prefetched, GINT_TO_POINTER(new_req->obj_id), GINT_TO_POINTER(1));
      }
      node = node->next;
    }

    my_free(sizeof(request_t), new_req);
    g_list_free(prefetch_list);
  }
}

void free_PG_prefetcher(prefetcher_t *prefetcher) {
  PG_params_t *PG_params = (PG_params_t *)prefetcher->params;

  g_hash_table_destroy(PG_params->cache_size_map);
  g_hash_table_destroy(PG_params->graph);
  g_hash_table_destroy(PG_params->prefetched);

  g_free(PG_params->past_requests);

  my_free(sizeof(PG_params_t), PG_params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  my_free(sizeof(prefetcher_t), prefetcher);
}

prefetcher_t *clone_PG_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  return create_PG_prefetcher(prefetcher->init_params, cache_size);
}

prefetcher_t *create_PG_prefetcher(const char *init_params, uint64_t cache_size) {
  PG_init_params_t *PG_init_params = my_malloc(PG_init_params_t);
  memset(PG_init_params, 0, sizeof(PG_init_params_t));

  set_PG_default_init_params(PG_init_params);
  if (init_params != NULL) {
    PG_parse_init_params(init_params, PG_init_params);
    check_params((PG_init_params));
  }

  PG_params_t *PG_params = my_malloc(PG_params_t);

  set_PG_params(PG_params, PG_init_params, cache_size);

  prefetcher_t *prefetcher = (prefetcher_t *)my_malloc(prefetcher_t);
  memset(prefetcher, 0, sizeof(prefetcher_t));
  prefetcher->params = PG_params;
  prefetcher->prefetch = PG_prefetch;
  prefetcher->handle_find = PG_handle_find;
  prefetcher->handle_insert = NULL;
  prefetcher->handle_evict = PG_handle_evict;
  prefetcher->free = free_PG_prefetcher;
  prefetcher->clone = clone_PG_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  my_free(sizeof(PG_init_params_t), PG_init_params);
  return prefetcher;
}

/******************** PG help function ********************/
static inline void _graphNode_destroy(gpointer data) {
  graphNode_t *graphNode = (graphNode_t *)data;
  g_hash_table_destroy(graphNode->graph);
  pqueue_free(graphNode->pq);
  g_free(graphNode);
}

/**
 1. insert the `req->obj_id` to the past_request_pointer.
 2. update the graph using `past_requests[past_request_pointer]` as the
 node and `node->past_requests[i]` as the directed arc.

 @param cache the cache struct
 @param req the request containing the request
 @return
 */
static inline void _PG_add_to_graph(cache_t *cache, const request_t *req) {
  PG_params_t *PG_params = (PG_params_t *)(cache->prefetcher->params);
  guint64 block, current_block = 0;
  char req_lbl[MAX_OBJ_ID_LEN], current_req_lbl[MAX_OBJ_ID_LEN];
  graphNode_t *graphNode = NULL;

  current_block = get_Nth_past_request_l(PG_params, PG_params->past_request_pointer);
  if (current_block) {
    graphNode = (graphNode_t *)g_hash_table_lookup(PG_params->graph, GINT_TO_POINTER(current_block));
  }

  // now update past requests
  set_Nth_past_request_l(PG_params, PG_params->past_request_pointer++, (guint64)(req->obj_id));

  PG_params->past_request_pointer = PG_params->past_request_pointer % PG_params->lookahead_range;

  if (!(current_req_lbl[0] || current_block)) {
    // this is the first request
    return;
  }

  if (graphNode == NULL) {
    if (!PG_params->stop_recording) {
      // current block is not in graph, insert
      gpointer key = GINT_TO_POINTER(current_block);
      graphNode = g_new0(graphNode_t, 1);
      graphNode->graph = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
      graphNode->pq = pqueue_init(2);
      graphNode->total_count = 0;
      g_hash_table_insert(PG_params->graph, key, graphNode);
      PG_params->cur_metadata_size += (8 + 8 * 3);
    } else {
      // no space for meta data
      return;
    }
  }

  for (int i = 0; i < PG_params->lookahead_range; i++) {
    graphNode->total_count++;

    block = get_Nth_past_request_l(PG_params, i);
    if (block == 0) break;

    pq_node_t *pq_node = (pq_node_t *)g_hash_table_lookup(graphNode->graph, GINT_TO_POINTER(block));
    if (pq_node) {
      // relation already exists
      pq_node->pri.pri++;
      pqueue_change_priority(graphNode->pq, pq_node->pri, pq_node);

#ifdef SANITY_CHECK
      if (pq_node->obj_id != block) {
        ERROR("pq node content not equal block\n");
      }
#endif

    } else {
      // there is no probability between current_block->block
      if (!PG_params->stop_recording) {
        pq_node_t *pq_node = g_new0(pq_node_t, 1);
        pq_node->obj_id = block;
        pq_node->pri.pri = 1;
        pqueue_insert(graphNode->pq, pq_node);
        g_hash_table_insert(graphNode->graph, GINT_TO_POINTER(pq_node->obj_id), pq_node);
        PG_params->cur_metadata_size += (8 + 8 * 3);
      } else {
        // no space for meta data
        return;
      }
    }
  }

  if (PG_params->max_metadata_size <= PG_params->cur_metadata_size) {
    PG_params->stop_recording = TRUE;
  }
}

/**
 get some objs which are associated with req->obj_id and their probability
 is higher than `prefetch_threshold`.

 @param cache the cache struct
 @param req the request containing the request
 @return list containing all objs that should be prefetched
 */
static inline GList *_PG_get_prefetch_list(cache_t *cache, const request_t *req) {
  PG_params_t *PG_params = (PG_params_t *)(cache->prefetcher->params);
  GList *list = NULL;
  graphNode_t *graphNode = g_hash_table_lookup(PG_params->graph, GINT_TO_POINTER(req->obj_id));

  if (graphNode == NULL) {
    return list;
  }

  GList *pq_node_list = NULL;
  while (1) {
    pq_node_t *pqNode = pqueue_pop(graphNode->pq);
    if (pqNode == NULL) {
      break;
    }
    if ((double)(pqNode->pri.pri) / (graphNode->total_count) > PG_params->prefetch_threshold) {
      list = g_list_prepend(list, GINT_TO_POINTER(pqNode->obj_id));
      pq_node_list = g_list_prepend(pq_node_list, pqNode);
    } else {
      //            printf("threshold %lf\n",
      //            (double)(pqNode->pri)/(graphNode->total_count));
      break;
    }
  }

  if (pq_node_list) {
    GList *node = pq_node_list;
    while (node) {
      pqueue_insert(graphNode->pq, node->data);
      node = node->next;
    }
  }
  g_list_free(pq_node_list);

  return list;
}

#ifdef __cplusplus
}
#endif

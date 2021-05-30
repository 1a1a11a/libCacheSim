//
//  PG.h
//  libCacheSim
//
//  Created by Juncheng on 11/20/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "PG.h"
#include "../dep/FIFO.h"
#include "../dep/LRU.h"
#include "Optimal.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************* priority queue structs and def **********************/

static int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr) {
  return (next.pri1 < curr.pri1);
}

static pqueue_pri_t get_pri(void *a) { return ((pq_node_t *) a)->pri; }

static void set_pri(void *a, pqueue_pri_t pri) { ((pq_node_t *) a)->pri = pri; }

static size_t get_pos(void *a) { return ((pq_node_t *) a)->pos; }

static void set_pos(void *a, size_t pos) { ((pq_node_t *) a)->pos = pos; }

/****************************** UTILS ************************************/
static void graphNode_destroy(gpointer data) {
  graphNode_t *graphNode = (graphNode_t *) data;
  g_hash_table_destroy(graphNode->graph);
  pqueue_free(graphNode->pq);
  g_free(graphNode);
}

static inline void _PG_add_to_graph(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  int i;
  guint64 block, current_block = 0;
  char req_lbl[MAX_OBJ_ID_LEN], current_req_lbl[MAX_OBJ_ID_LEN];
  graphNode_t *graphNode = NULL;

  if (PG->obj_id_type == OBJ_ID_NUM) {
    current_block =
      get_Nth_past_request_l(PG_params, PG_params->past_request_pointer);
    if (current_block)
      graphNode =
        (graphNode_t *) g_hash_table_lookup(PG_params->graph, &current_block);
  }
  if (PG->obj_id_type == OBJ_ID_STR) {
    get_Nth_past_request_c(PG_params, PG_params->past_request_pointer,
                           current_req_lbl);
    if (current_req_lbl[0])
      graphNode =
        (graphNode_t *) g_hash_table_lookup(PG_params->graph, current_req_lbl);
  }

  // now update past requests
  if (PG->obj_id_type == OBJ_ID_NUM) {
    set_Nth_past_request_l(PG_params, PG_params->past_request_pointer++,
                           *(guint64 *) (req->obj_id_ptr));
  }

  if (PG->obj_id_type == OBJ_ID_STR) {
    set_Nth_past_request_c(PG_params, PG_params->past_request_pointer++,
                           (req->obj_id_ptr));
  }
  PG_params->past_request_pointer =
    PG_params->past_request_pointer % PG_params->lookahead;

  if (!(current_req_lbl[0] || current_block))
    // this is the first request
    return;

  if (graphNode == NULL) {
    if (!PG_params->stop_recording) {
      // current block is not in graph, insert
      gpointer key = GSIZE_TO_POINTER(current_block);
      graphNode = g_new0(graphNode_t, 1);

      if (req->obj_id_type == OBJ_ID_NUM) {
        graphNode->graph = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                 g_free,
                                                 g_free);
      } else {
        key = (gpointer) g_strdup(current_req_lbl);
        graphNode->graph = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free,
                                                 g_free);
      }
      graphNode->pq = pqueue_init(2, cmp_pri, get_pri, set_pri, get_pos, set_pos);
      graphNode->total_count = 0;
      g_hash_table_insert(PG_params->graph, key, graphNode);
      PG_params->meta_data_size += (8 + 8 * 3);
    } else
      // no space for meta data
      return;
  }

  for (i = 0; i < PG_params->lookahead; i++) {
    graphNode->total_count++;
    if (PG->obj_id_type == OBJ_ID_NUM) {
      block = get_Nth_past_request_l(PG_params, i);
      if (block == 0)
        break;

      pq_node_t *pq_node =
        (pq_node_t *) g_hash_table_lookup(graphNode->graph, &block);
      if (pq_node) {
        // relation already exists
        pq_node->pri.pri1++;
        pqueue_change_priority(graphNode->pq, pq_node->pri, pq_node);

#ifdef SANITY_CHECK
        if (*((gint64 *)(pq_node->obj_id)) != (gint64)block)
          ERROR("pq node content not equal block\n");
#endif
      } else {
        // there is no probability between current_block->block
        if (!PG_params->stop_recording) {
          pq_node_t *pq_node = g_new0(pq_node_t, 1);
          pq_node->obj_id_type = OBJ_ID_NUM;
          pq_node->obj_id = GSIZE_TO_POINTER(block);
          pq_node->pri.pri1 = 1;
          pqueue_insert(graphNode->pq, pq_node);
          g_hash_table_insert(graphNode->graph, pq_node->obj_id, pq_node);
          PG_params->meta_data_size += (8 + 8 * 3);
        } else
          return;
      }
    }
    if (PG->obj_id_type == OBJ_ID_STR) {
      get_Nth_past_request_c(PG_params, i, req_lbl);
      if (req_lbl[0] == 0)
        break;

      pq_node_t *pq_node =
        (pq_node_t *) g_hash_table_lookup(graphNode->graph, req_lbl);
      if (pq_node) {
        // relation already exists
        pq_node->pri.pri1++;
        pqueue_change_priority(graphNode->pq, pq_node->pri, pq_node);

        // sanity check
        if (strcmp((gchar *) (pq_node->obj_id), req_lbl) != 0)
          printf("ERROR pq node content not equal req\n");
      } else {
        // there is no probability between current_block->block
        if (!PG_params->stop_recording) {
          gpointer key = (gpointer) g_strdup((gchar *) (req_lbl));

          pq_node_t *pq_node = g_new0(pq_node_t, 1);
          pq_node->obj_id_type = OBJ_ID_STR;
          pq_node->obj_id = key;
          pq_node->pri.pri1 = 1;
          pqueue_insert(graphNode->pq, pq_node);
          g_hash_table_insert(graphNode->graph, key, pq_node);
          PG_params->meta_data_size += (8 + 8 * 3);
        } else
          return;
      }
    }
  }

  if (PG_params->init_size * PG_params->max_meta_data <=
      PG_params->meta_data_size / PG_params->block_size)
    PG_params->stop_recording = TRUE;

  PG->size = PG_params->init_size -
                   PG_params->meta_data_size / 1024 / PG_params->block_size;
  PG_params->cache->size =
    PG_params->init_size - PG_params->meta_data_size / PG_params->block_size;
}

static inline GList *PG_get_prefetch(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  GList *list = NULL;
  graphNode_t *graphNode = g_hash_table_lookup(PG_params->graph, req->obj_id_ptr);

  if (graphNode == NULL)
    return list;

  GList *pq_node_list = NULL;
  while (1) {
    pq_node_t *pqNode = pqueue_pop(graphNode->pq);
    if (pqNode == NULL)
      break;
    if ((double) (pqNode->pri.pri1) / (graphNode->total_count) >
        PG_params->prefetch_threshold) {
      list = g_list_prepend(list, pqNode->obj_id);
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

// static void printHashTable (gpointer key, gpointer value, gpointer
// user_data){
//    printf("key %s, value %p\n", (char*)key, value);
//}

/************************** probability graph ****************************/

void _PG_insert(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  PG_params->cache->_insert(PG_params->cache, req);
}

gboolean PG_check(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  return PG_params->cache->check(PG_params->cache, req);
}

void _PG_update(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  PG_params->cache->_update(PG_params->cache, req);
}

void _PG_evict(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  g_hash_table_remove(PG_params->prefetched, req->obj_id_ptr);

  PG_params->cache->_evict(PG_params->cache, req);
}

gpointer _PG_evict_with_return(cache_t *PG, request_t *req) {
  /** evict one element and return the evicted element, needs to free the memory
   * of returned data **/
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  g_hash_table_remove(PG_params->prefetched, req->obj_id_ptr);
  return PG_params->cache->evict_with_return(PG_params->cache, req);
}

gboolean PG_add(cache_t *PG, request_t *req) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  _PG_add_to_graph(PG, req);

  if (g_hash_table_contains(PG_params->prefetched, req->obj_id_ptr)) {
    PG_params->num_of_hit++;
    g_hash_table_remove(PG_params->prefetched, req->obj_id_ptr);
    if (g_hash_table_contains(PG_params->prefetched, req->obj_id_ptr))
      fprintf(stderr, "ERROR found prefetch\n");
  }

  gboolean retval;

  // original add
  if (PG_check(PG, req)) {
    _PG_update(PG, req);
    retval = TRUE;
  } else {
    _PG_insert(PG, req);
    while ((long)PG_get_size(PG) > PG->size)
      _PG_evict(PG, req);
    retval = FALSE;
  }

  // begin prefetching
  GList *prefetch_list = PG_get_prefetch(PG, req);
  if (prefetch_list) {
    GList *node = prefetch_list;
    while (node) {
      req->obj_id_ptr = node->data;
      if (!PG_check(PG, req)) {
        PG_params->cache->_insert(PG_params->cache, req);
        PG_params->num_of_prefetch += 1;

        gpointer item_p = req->obj_id_ptr;
        if (req->obj_id_type == OBJ_ID_STR)
          item_p = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));

        g_hash_table_insert(PG_params->prefetched, item_p, GINT_TO_POINTER(1));
      }
      node = node->next;
    }

    while ((long)PG_get_size(PG) > PG->size)
      _PG_evict(PG, req);
    g_list_free(prefetch_list);
  }
  PG->ts += 1;
  return retval;
}

gboolean PG_add_only(cache_t *PG, request_t *req) {
  gboolean retval;
  if (PG_check(PG, req)) {
    _PG_update(PG, req);
    retval = TRUE;
  } else {
    _PG_insert(PG, req);
    while ((long)PG_get_size(PG) > PG->size)
      _PG_evict(PG, req);
    retval = FALSE;
  }
  PG->ts += 1;
  return retval;
}

//gboolean PG_add_withsize(cache_t *eviction, request_t *req) {
//  int i;
//  gboolean ret_val;
//
//  *(gint64 *)(req->obj_id_ptr) =
//      (gint64)(*(gint64 *)(req->obj_id_ptr) * req->disk_sector_size /
//               eviction->block_size);
//  ret_val = PG_add(eviction, req);
//
//  int n = (int)ceil((double)req->size / eviction->block_size);
//
//  for (i = 0; i < n - 1; i++) {
//    (*(guint64 *)(req->obj_id_ptr))++;
//    PG_add_only(eviction, req);
//  }
//  *(gint64 *)(req->obj_id_ptr) -= (n - 1);
//  eviction->ts += 1;
//  return ret_val;
//}

void PG_destroy(cache_t *PG) {
  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  PG_params->cache->destroy(PG_params->cache);
  g_hash_table_destroy(PG_params->graph);
  g_hash_table_destroy(PG_params->prefetched);

  if (PG->obj_id_type == OBJ_ID_STR) {
    int i;
    for (i = 0; i < PG_params->lookahead; i++)
      g_free(((char **) (PG_params->past_requests))[i]);
  }
  g_free(PG_params->past_requests);
  cache_destroy(PG);
}

void PG_destroy_unique(cache_t *PG) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the eviction, freeing these resources won't affect
   other caches copied from original eviction
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  PG_params_t *PG_params = (PG_params_t *) (PG->cache_params);
  PG_params->cache->destroy(PG_params->cache);
  g_hash_table_destroy(PG_params->graph);
  g_hash_table_destroy(PG_params->prefetched);

  if (PG->obj_id_type == OBJ_ID_STR) {
    int i;
    for (i = 0; i < PG_params->lookahead; i++)
      g_free(((char **) (PG_params->past_requests))[i]);
  }
  g_free(PG_params->past_requests);
  g_free(PG->cache_params);
  g_free(PG->core);
  g_free(PG);
}

cache_t *PG_init(guint64 size, obj_id_t obj_id_type, void *params) {

  cache_t *cache = cache_init("PG", size, obj_id_type);
  cache->cache_params = g_new0(PG_params_t, 1);
  PG_params_t *PG_params = (PG_params_t *) (cache->cache_params);
  PG_init_params_t *init_params = (PG_init_params_t *) params;

  cache->cache_init = PG_init;
  cache->destroy = PG_destroy;
  cache->destroy_unique = PG_destroy_unique;
  cache->add = PG_add;
  cache->check = PG_check;
  cache->_insert = _PG_insert;
  cache->_update = _PG_update;
  cache->_evict = _PG_evict;
  cache->evict_with_return = _PG_evict_with_return;
//  eviction->add_only = PG_add_only;
//  eviction->add_withsize = PG_add_withsize;

  cache->get_used_size = PG_get_size;
  cache->cache_init_params = params;

  PG_params->lookahead = init_params->lookahead;
  PG_params->prefetch_threshold = init_params->prefetch_threshold;
  PG_params->max_meta_data = init_params->max_meta_data;
  PG_params->init_size = size;
  PG_params->block_size = init_params->block_size;
  PG_params->stop_recording = FALSE;

  if (strcmp(init_params->cache_type, "LRU") == 0)
    PG_params->cache = LRU_init(size, obj_id_type, NULL);
  else if (strcmp(init_params->cache_type, "FIFO") == 0)
    PG_params->cache = FIFO_init(size, obj_id_type, NULL);
  else if (strcmp(init_params->cache_type, "Optimal") == 0) {
    struct Optimal_init_params *Optimal_init_params =
      g_new(struct Optimal_init_params, 1);
    Optimal_init_params->reader = NULL;
    PG_params->cache = NULL;
  } else {
    fprintf(stderr, "can't recognize eviction obj_id_type: %s\n",
            init_params->cache_type);
    PG_params->cache = LRU_init(size, obj_id_type, NULL);
  }

  if (obj_id_type == OBJ_ID_NUM) {
    PG_params->graph =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, graphNode_destroy);
    PG_params->prefetched = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
    PG_params->past_requests = g_new0(guint64, PG_params->lookahead);
  } else if (obj_id_type == OBJ_ID_STR) {
    PG_params->graph =
      g_hash_table_new_full(g_str_hash, g_str_equal,
                            g_free, graphNode_destroy);
    PG_params->prefetched = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
    PG_params->past_requests = g_new0(char *, PG_params->lookahead);
    int i;
    for (i = 0; i < PG_params->lookahead; i++)
      ((char **) (PG_params->past_requests))[i] =
        g_new0(char, MAX_OBJ_ID_LEN);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  return cache;
}

guint64 PG_get_size(cache_t *cache) {
  PG_params_t *PG_params = (PG_params_t *) (cache->cache_params);
  return (guint64) PG_params->cache->get_used_size(PG_params->cache);
}

#ifdef __cplusplus
}
#endif

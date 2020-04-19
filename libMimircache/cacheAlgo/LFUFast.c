//
//  LFUFast.c
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/* this module uses linkedlist to order requests,
 * which is O(1) at each request, which should be preferred in most cases
 * the drawback of this implementation is the memory usage, because two pointers
 * are associated with each obj_id
 *
 * this LFUFast clear the frequenct of an obj_id after evicting from cacheAlgo
 * this LFUFast is LFU_LRU, which choose LRU end when more than one items have
 * the same smallest freq
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "LFUFast.h"
#include "../../include/mimircache/cacheOp.h"


/********************************** LFUFast ************************************/
gboolean _LFUFast_verify(cache_t *LFUFast) {

  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (LFUFast->cache_params);
  GList *mnode_list = g_queue_peek_head_link(LFUFast_params->main_list);
  main_list_node_data_t *mnode_data;
  //    branch_list_node_data_t *bnode_data;
  guint64 current_size = 0;
  while (mnode_list) {
    mnode_data = mnode_list->data;
    current_size += g_queue_get_length(mnode_data->queue);
    printf("%u\t", g_queue_get_length(mnode_data->queue));
    mnode_list = mnode_list->next;
  }
  printf("\n");
  if (g_hash_table_size(LFUFast_params->hashtable) == current_size)
    return TRUE;
  else {
    ERROR("hashtable size %u, queue accu size %lu\n",
          g_hash_table_size(LFUFast_params->hashtable),
          (unsigned long) current_size);
    return FALSE;
  }
}

void _LFUFast_insert(cache_t *LFUFast, request_t *req) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (LFUFast->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  branch_list_node_data_t *bnode_data = g_new0(branch_list_node_data_t, 1);
  bnode_data->key = key;
  GList *list_node = g_list_append(NULL, bnode_data);

  g_hash_table_insert(LFUFast_params->hashtable, (gpointer) key,
                      (gpointer) list_node);

  main_list_node_data_t *mnode_data;
  if (LFUFast_params->min_freq != 1) {
    // initial
    if (LFUFast_params->min_freq < 1 &&
        LFUFast_params->main_list->length != 0) {
      WARNING("LFU initialization error\n");
    }
    mnode_data = g_new0(main_list_node_data_t, 1);
    mnode_data->freq = 1;
    mnode_data->queue = g_queue_new();
    g_queue_push_head(LFUFast_params->main_list, mnode_data);
    LFUFast_params->min_freq = 1;
  } else {
    mnode_data = (main_list_node_data_t *) (g_queue_peek_head(
      LFUFast_params->main_list));
#ifdef SANITY_CHECK
    if (mnode_data->freq != 1) {
      ERROR("first main node freq is not 1, is %d\n", mnode_data->freq);
      exit(1);
    }
#endif
  }

  g_queue_push_tail_link(mnode_data->queue, list_node);
  bnode_data->main_list_node =
    g_queue_peek_head_link(LFUFast_params->main_list);
}

gboolean LFUFast_check(cache_t *cache, request_t *req) {
  return g_hash_table_contains(
    ((LFUFast_params_t *) (cache->cache_params))->hashtable,
    (gconstpointer) (req->obj_id_ptr));
}

void _LFUFast_update(cache_t *cache, request_t *req) {
  /* find the given bnode_data, remove from current list,
   * insert into next main node */
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);

  GList *list_node = g_hash_table_lookup(LFUFast_params->hashtable,
                                         (gconstpointer) (req->obj_id_ptr));

  branch_list_node_data_t *bnode_data = list_node->data;
  main_list_node_data_t *mnode_data = bnode_data->main_list_node->data;

  // remove from current main_list_node
  g_queue_unlink(mnode_data->queue, list_node);

  // check whether there is next
  gboolean exist_next =
    (bnode_data->main_list_node->next == NULL) ? FALSE : TRUE;

  // check whether next is freq+1
  main_list_node_data_t *mnode_next_data =
    exist_next ? bnode_data->main_list_node->next->data : NULL;
  if (exist_next && mnode_next_data->freq == mnode_data->freq + 1) {
    // insert to this main list node
    g_queue_push_tail_link(mnode_next_data->queue, list_node);
  } else {
#ifdef SANITY_CHECK
    if (exist_next && mnode_next_data->freq <= mnode_data->freq) {
      ERROR("mnode next freq %d, current freq %d\n", mnode_next_data->freq,
            mnode_data->freq);
      exit(-1);
    }
#endif
    // create a new main list node, insert in between
    main_list_node_data_t *new_mnode_data = g_new0(main_list_node_data_t, 1);
    new_mnode_data->freq = mnode_data->freq + 1;
    new_mnode_data->queue = g_queue_new();
    g_queue_push_tail_link(new_mnode_data->queue, list_node);
    // insert mnode
    g_queue_insert_after(LFUFast_params->main_list, bnode_data->main_list_node,
                         new_mnode_data);
  }
  bnode_data->main_list_node = bnode_data->main_list_node->next;
}

void _LFUFast_evict(cache_t *cache, request_t *req) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);

  GList *mnode = g_queue_peek_head_link(LFUFast_params->main_list);
  main_list_node_data_t *mnode_data =
    g_queue_peek_head(LFUFast_params->main_list);

  // find the first main list node that has an non-empty queue
  while (g_queue_is_empty(mnode_data->queue)) {
    mnode = mnode->next;
    mnode_data = mnode->data;
  }
  branch_list_node_data_t *bnode_data = g_queue_pop_head(mnode_data->queue);
  g_hash_table_remove(LFUFast_params->hashtable,
                      (gconstpointer) (bnode_data->key));

  g_free(bnode_data);
}

gpointer _LFUFast_evict_with_return(cache_t *cache, request_t *req) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);

  GList *mnode = g_queue_peek_head_link(LFUFast_params->main_list);
  main_list_node_data_t *mnode_data =
    g_queue_peek_head(LFUFast_params->main_list);

  // find the first main list node that has an non-empty queue
  while (g_queue_is_empty(mnode_data->queue)) {
    mnode = mnode->next;
    mnode_data = mnode->data;
  }

  branch_list_node_data_t *bnode_data = g_queue_pop_head(mnode_data->queue);

  // save evicted key
  gpointer evicted_key = bnode_data->key;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) bnode_data->key);
  }
  g_hash_table_remove(LFUFast_params->hashtable,
                      (gconstpointer) (bnode_data->key));
  return evicted_key;
}

gboolean LFUFast_add(cache_t *cache, request_t *req) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);
  gboolean retval;

  if (LFUFast_check(cache, req)) {
    _LFUFast_update(cache, req);
    retval = TRUE;
  } else {
    _LFUFast_insert(cache, req);
    if ((long) g_hash_table_size(LFUFast_params->hashtable) > cache->core->size)
      _LFUFast_evict(cache, req);
    retval = FALSE;
  }
  cache->core->req_cnt += 1;
  return retval;
}

gboolean LFUFast_add_only(cache_t *cache, request_t *req) {
  return LFUFast_add(cache, req);
}

gboolean LFUFast_add_withsize(cache_t *cache, request_t *req) {
  gint64 original_lbn = *(gint64 *) (req->obj_id_ptr);
  gboolean ret_val;
  ret_val = LFUFast_add(cache, req);

  *(gint64 *) (req->obj_id_ptr) = original_lbn;
  cache->core->req_cnt += 1;
  return ret_val;
}

void free_main_list_node_data(gpointer data) {
  main_list_node_data_t *mnode_data = data;
  g_queue_free_full(mnode_data->queue, g_free);
  g_free(data);
}

void LFUFast_destroy(cache_t *cache) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);

  g_queue_free_full(LFUFast_params->main_list, free_main_list_node_data);
  g_hash_table_destroy(LFUFast_params->hashtable);

  cache_struct_free(cache);
}

void LFUFast_destroy_unique(cache_t *cache) {
  /* the difference between destroy_cloned_cache and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in LFUFast, next_access should not be freed in destroy_cloned_cache,
   because it is shared between different caches copied from the original one.
   */
  LFUFast_destroy(cache);
}

cache_t *LFUFast_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("LFUFast", size, obj_id_type);
  LFUFast_params_t *LFUFast_params = g_new0(LFUFast_params_t, 1);
  cache->cache_params = (void *) LFUFast_params;


  if (obj_id_type == OBJ_ID_NUM) {
    LFUFast_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    LFUFast_params->hashtable = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  LFUFast_params->min_freq = 0;
  LFUFast_params->main_list = g_queue_new();

  return cache;
}

guint64 LFUFast_get_size(cache_t *cache) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);
  return (guint64) g_hash_table_size(LFUFast_params->hashtable);
}

void LFUFast_remove_obj(cache_t *cache, void *data_to_remove) {
  LFUFast_params_t *LFUFast_params =
    (LFUFast_params_t *) (cache->cache_params);
  GList *blist_node =
    g_hash_table_lookup(LFUFast_params->hashtable, data_to_remove);

  branch_list_node_data_t *bnode_data = blist_node->data;
  main_list_node_data_t *mnode_data = bnode_data->main_list_node->data;
  g_queue_unlink(mnode_data->queue, blist_node);

  g_hash_table_remove(LFUFast_params->hashtable,
                      (gconstpointer) (data_to_remove));
}

#ifdef __cplusplus
}
#endif

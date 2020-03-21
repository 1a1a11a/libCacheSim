//
//  myLRU.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "myLRU.h"

#ifdef __cplusplus
extern "C" {
#endif

void __myLRU_insert_element(cache_t *myLRU, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(myLRU->cache_params);

  gpointer key;
  if (req->obj_id_type == OBJ_ID_NUM) {
    key = (gpointer)g_new(guint64, 1);
    *(guint64 *)key = *(guint64 *)(req->obj_id_ptr);
  } else {
    key = (gpointer)g_strdup((gchar *)(req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = key;

  g_queue_push_tail_link(myLRU_params->list, node);
  g_hash_table_insert(myLRU_params->hashtable, (gpointer)key, (gpointer)node);
}

gboolean myLRU_check_element(cache_t *cache, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);
  return g_hash_table_contains(myLRU_params->hashtable, req->obj_id_ptr);
}

void __myLRU_update_element(cache_t *cache, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);
  GList *node =
      (GList *)g_hash_table_lookup(myLRU_params->hashtable, req->obj_id_ptr);
  g_queue_unlink(myLRU_params->list, node);
  g_queue_push_tail_link(myLRU_params->list, node);
}

void __myLRU_evict_element(cache_t *myLRU, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(myLRU->cache_params);

  if (myLRU->core->record_level == 2) { // compare to Oracle
    while (myLRU_params->ts > (gint64)g_array_index(myLRU->core->bp->array, guint64,
                                                  myLRU->core->bp_pos)) {
      if ((long)g_array_index(myLRU->core->bp->array, guint64,
                              myLRU->core->bp_pos) -
              (long)g_array_index(myLRU->core->bp->array, guint64,
                                  myLRU->core->bp_pos - 1) !=
          0) {

        myLRU->core->evict_err_array[myLRU->core->bp_pos - 1] =
            myLRU->core->evict_err /
            (g_array_index(myLRU->core->bp->array, guint64, myLRU->core->bp_pos) -
             g_array_index(myLRU->core->bp->array, guint64,
                           myLRU->core->bp_pos - 1));
        myLRU->core->evict_err = 0;
      } else
        myLRU->core->evict_err_array[myLRU->core->bp_pos - 1] = 0;

      myLRU->core->evict_err = 0;
      myLRU->core->bp_pos++;
    }

    if (myLRU_params->ts ==
        (long)g_array_index(myLRU->core->bp->array, guint64, myLRU->core->bp_pos)) {
      myLRU->core->evict_err_array[myLRU->core->bp_pos - 1] =
          (double)myLRU->core->evict_err /
          (g_array_index(myLRU->core->bp->array, guint64, myLRU->core->bp_pos) -
           g_array_index(myLRU->core->bp->array, guint64, myLRU->core->bp_pos - 1));
      myLRU->core->evict_err = 0;
      myLRU->core->bp_pos++;
    }

    gpointer data = g_queue_peek_head(myLRU_params->list);
    if (req->obj_id_type == OBJ_ID_NUM) {
      if (*(guint64 *)(data) !=
          ((guint64 *)myLRU->core->oracle)[myLRU_params->ts]) {
        printf("error at %lu, myLRU: %lu, Optimal: %lu\n",
               (unsigned long)myLRU_params->ts, *(unsigned long *)(data),
               ((unsigned long *)myLRU->core->oracle)[myLRU_params->ts]);
        myLRU->core->evict_err++;
      } else
        printf("no error at %lu: %lu, %lu\n", (unsigned long)myLRU_params->ts,
               *(unsigned long *)(data),
               *(unsigned long *)(g_queue_peek_tail(myLRU_params->list)));
      gpointer data_oracle = g_hash_table_lookup(
          myLRU_params->hashtable,
          (gpointer) & ((guint64 *)myLRU->core->oracle)[myLRU_params->ts]);
      g_queue_delete_link(myLRU_params->list, (GList *)data_oracle);
      g_hash_table_remove(myLRU_params->hashtable,
                          (gpointer) &
                              ((guint64 *)myLRU->core->oracle)[myLRU_params->ts]);
    } else {
      if (strcmp((gchar *)data,
                 ((gchar **)(myLRU->core->oracle))[myLRU_params->ts]) != 0)
        myLRU->core->evict_err++;
      gpointer data_oracle = g_hash_table_lookup(
          myLRU_params->hashtable,
          (gpointer)((gchar **)myLRU->core->oracle)[myLRU_params->ts]);
      g_hash_table_remove(
          myLRU_params->hashtable,
          (gpointer)((gchar **)myLRU->core->oracle)[myLRU_params->ts]);
      g_queue_remove(myLRU_params->list, ((GList *)data_oracle)->data);
    }

  }

  else if (myLRU->core->record_level == 1) {
    // record eviction list

    gpointer data = g_queue_pop_head(myLRU_params->list);
    if (req->obj_id_type == OBJ_ID_NUM) {
      ((guint64 *)(myLRU->core->eviction_array))[myLRU_params->ts] =
          *(guint64 *)(data);
    } else {
      gchar *key = g_strdup((gchar *)(data));
      ((gchar **)(myLRU->core->eviction_array))[myLRU_params->ts] = key;
    }

    g_hash_table_remove(myLRU_params->hashtable, (gconstpointer)data);
  }

  else {
    gpointer data = g_queue_pop_head(myLRU_params->list);
    g_hash_table_remove(myLRU_params->hashtable, (gconstpointer)data);
  }
}

gpointer __myLRU__evict_with_return(cache_t *myLRU, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  struct myLRU_params *myLRU_params = (struct myLRU_params *)(myLRU->cache_params);

  gpointer data = g_queue_pop_head(myLRU_params->list);

  gpointer evicted_key;
  if (req->obj_id_type == OBJ_ID_NUM) {
    evicted_key = (gpointer)g_new(guint64, 1);
    *(guint64 *)evicted_key = *(guint64 *)(data);
  } else {
    evicted_key = (gpointer)g_strdup((gchar *)data);
  }

  g_hash_table_remove(myLRU_params->hashtable, (gconstpointer)data);
  return evicted_key;
}

gboolean myLRU_add_element(cache_t *cache, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);
  gboolean retval;
  if (myLRU_check_element(cache, req)) {
    __myLRU_update_element(cache, req);
    myLRU_params->ts++;
    retval = TRUE;
  } else {
    __myLRU_insert_element(cache, req);
    if ((long)g_hash_table_size(myLRU_params->hashtable) > cache->core->size)
      __myLRU_evict_element(cache, req);
    myLRU_params->ts++;
    retval = FALSE;
  }
  cache->core->ts += 1;
  return retval;
}

gboolean myLRU_add_element_only(cache_t *cache, request_t *req) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);
  gboolean retval;
  if (myLRU_check_element(cache, req)) {
    __myLRU_update_element(cache, req);
    retval = TRUE;
  } else {
    __myLRU_insert_element(cache, req);
    while ((long)g_hash_table_size(myLRU_params->hashtable) > cache->core->size)
      __myLRU_evict_element(cache, req);
    retval= FALSE;
  }

  cache->core->ts += 1;
  return retval;
}

gboolean myLRU_add_element_withsize(cache_t *cache, request_t *req) {
  int i;
  gboolean ret_val;

  *(gint64 *)(req->obj_id_ptr) =
      (gint64)(*(gint64 *)(req->obj_id_ptr) * req->disk_sector_size /
               cache->core->block_size);
  ret_val = myLRU_add_element(cache, req);

  int n = (int)ceil((double)req->size / cache->core->block_size);

  for (i = 0; i < n - 1; i++) {
    (*(guint64 *)(req->obj_id_ptr))++;
    myLRU_add_element_only(cache, req);
  }
  *(gint64 *)(req->obj_id_ptr) -= (n - 1);

  cache->core->ts += 1;
  return ret_val;
}

void myLRU_destroy(cache_t *cache) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);

  //    g_queue_free(myLRU_params->list);                 // Jason: should call
  //    g_queue_free_full to free the memory of node content
  // 0921
  g_queue_free(myLRU_params->list);
  g_hash_table_destroy(myLRU_params->hashtable);
  cache_destroy(cache);
}

void myLRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cache, freeing these resources won't affect
   other caches copied from original cache
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  myLRU_destroy(cache);
}

/*-----------------------------------------------------------------------------
 *
 * myLRU_init --
 *      initialize a myLRU cache
 *
 * Input:
 *      size:       cache size
 *      obj_id_type:  the obj_id_type of data, currently support l for long or c
 *for string block_size: the basic unit size of block, used for profiling with
 *size if not profiling with size, this is 0 params:     params used for
 *initialization, NULL for myLRU
 *
 * Return:
 *      a myLRU cache struct
 *
 *-----------------------------------------------------------------------------
 */
cache_t *myLRU_init(guint64 size, obj_id_t obj_id_type, guint64 block_size,
                  void *params) {
  cache_t *cache = cache_init(size, obj_id_type, block_size);
  cache->cache_params = g_new0(struct myLRU_params, 1);
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);

  cache->core->cache_init = myLRU_init;
  cache->core->destroy = myLRU_destroy;
  cache->core->destroy_unique = myLRU_destroy_unique;
  cache->core->add_element = myLRU_add_element;
  cache->core->check_element = myLRU_check_element;
  cache->core->__insert_element = __myLRU_insert_element;
  cache->core->__update_element = __myLRU_update_element;
  cache->core->__evict_element = __myLRU_evict_element;
  cache->core->__evict_with_return = __myLRU__evict_with_return;
  cache->core->get_current_size = myLRU_get_size;
  cache->core->remove_element = myLRU_remove_element;
  cache->core->cache_init_params = NULL;
  cache->core->add_element_only = myLRU_add_element_only;
  cache->core->add_element_withsize = myLRU_add_element_withsize;

  if (obj_id_type == OBJ_ID_NUM) {
    myLRU_params->hashtable = g_hash_table_new_full(
        g_int64_hash, g_int64_equal, g_free, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    myLRU_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  myLRU_params->list = g_queue_new();

  return cache;
}

void myLRU_remove_element(cache_t *cache, void *data_to_remove) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);

  gpointer data = g_hash_table_lookup(myLRU_params->hashtable, data_to_remove);
  if (!data) {
    fprintf(stderr, "myLRU_remove_element: data to remove is not in the cache\n");
    exit(1);
  }
  g_queue_delete_link(myLRU_params->list, (GList *)data);
  g_hash_table_remove(myLRU_params->hashtable, data_to_remove);
}

guint64 myLRU_get_size(cache_t *cache) {
  struct myLRU_params *myLRU_params = (struct myLRU_params *)(cache->cache_params);
  return (guint64)g_hash_table_size(myLRU_params->hashtable);
}

#ifdef __cplusplus
}
#endif

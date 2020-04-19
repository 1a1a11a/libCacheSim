//
// Created by Juncheng Yang on 11/22/19.
//

#include "slabLRC.h"


#ifdef __cplusplus
extern "C" {
#endif

void _slabLRC_insert(cache_t *slabLRC, request_t *req) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (slabLRC->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = key;

  g_queue_push_tail_link(slabLRC_params->list, node);
  g_hash_table_insert(slabLRC_params->hashtable, (gpointer) key, (gpointer) node);
}

gboolean slabLRC_check(cache_t *cache, request_t *req) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);
  return g_hash_table_contains(slabLRC_params->hashtable, req->obj_id_ptr);
}

void _slabLRC_update(cache_t *cache, request_t *req) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);
  GList *node =
    (GList *) g_hash_table_lookup(slabLRC_params->hashtable, req->obj_id_ptr);
  g_queue_unlink(slabLRC_params->list, node);
  g_queue_push_tail_link(slabLRC_params->list, node);
}

void _slabLRC_evict(cache_t *slabLRC, request_t *req) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (slabLRC->cache_params);
  gpointer data = g_queue_pop_head(slabLRC_params->list);
  g_hash_table_remove(slabLRC_params->hashtable, (gconstpointer) data);

}

gpointer _slabLRC_evict_with_return(cache_t *slabLRC, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (slabLRC->cache_params);

  gpointer data = g_queue_pop_head(slabLRC_params->list);

  gpointer evicted_key = data;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) data);
  }

  g_hash_table_remove(slabLRC_params->hashtable, (gconstpointer) data);
  return evicted_key;
}

gboolean slabLRC_add(cache_t *cache, request_t *req) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);
  gboolean retval;
  if (slabLRC_check(cache, req)) {
    _slabLRC_update(cache, req);
    slabLRC_params->ts++;
    retval = TRUE;
  } else {
    _slabLRC_insert(cache, req);
    if ((long) g_hash_table_size(slabLRC_params->hashtable) > cache->core->size)
      _slabLRC_evict(cache, req);
    slabLRC_params->ts++;
    retval = FALSE;
  }
  cache->core->ts += 1;
  return retval;
}


void slabLRC_destroy(cache_t *cache) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);

  //    g_queue_free(slabLRC_params->list);                 // Jason: should call
  //    g_queue_free_full to free the memory of node content
  // 0921
  g_queue_free(slabLRC_params->list);
  g_hash_table_destroy(slabLRC_params->hashtable);
  cache_destroy(cache);
}

void slabLRC_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  slabLRC_destroy(cache);
}

/*-----------------------------------------------------------------------------
 *
 * slabLRC_init --
 *      initialize a slabLRC cacheAlgo
 *
 * Input:
 *      size:       cacheAlgo size
 *      obj_id_type:  the obj_id_type of data, currently support l for long or c
 *for string block_size: the basic unit size of block, used for profiling with
 *size if not evaluate with size, this is 0 params:     params used for
 *initialization, NULL for slabLRC
 *
 * Return:
 *      a slabLRC cacheAlgo struct
 *
 *-----------------------------------------------------------------------------
 */
cache_t *slabLRC_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("slabLRC", size, obj_id_type);
  cache->cache_params = g_new0(struct slabLRC_params, 1);
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);

//  cacheAlgo->core->cache_init = slabLRC_init;
//  cacheAlgo->core->destroy = slabLRC_destroy;
//  cacheAlgo->core->destroy_unique = slabLRC_destroy_unique;
//  cacheAlgo->core->add = slabLRC_add;
//  cacheAlgo->core->check = slabLRC_check;
//  cacheAlgo->core->_insert = _slabLRC_insert;
//  cacheAlgo->core->_update = _slabLRC_update;
//  cacheAlgo->core->_evict = _slabLRC_evict;
//  cacheAlgo->core->evict_with_return = _slabLRC_evict_with_return;
//  cacheAlgo->core->get_used_size = slabLRC_get_size;
//  cacheAlgo->core->remove_obj = slabLRC_remove_obj;
//  cacheAlgo->core->cache_init_params = NULL;
//  cacheAlgo->core->add_only = slabLRC_add_only;
//  cacheAlgo->core->add_withsize = slabLRC_add_withsize;

  if (obj_id_type == OBJ_ID_NUM) {
    slabLRC_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    slabLRC_params->hashtable = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  slabLRC_params->list = g_queue_new();

  return cache;
}

void slabLRC_remove_obj(cache_t *cache, void *data_to_remove) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);

  gpointer data = g_hash_table_lookup(slabLRC_params->hashtable, data_to_remove);
  if (!data) {
    fprintf(stderr, "slabLRC_remove_obj: data to remove is not in the cacheAlgo\n");
    exit(1);
  }
  g_queue_delete_link(slabLRC_params->list, (GList *) data);
  g_hash_table_remove(slabLRC_params->hashtable, data_to_remove);
}

guint64 slabLRC_get_current_size(cache_t *cache) {
  struct slabLRC_params *slabLRC_params = (struct slabLRC_params *) (cache->cache_params);
  return (guint64) g_hash_table_size(slabLRC_params->hashtable);
}

#ifdef __cplusplus
}
#endif

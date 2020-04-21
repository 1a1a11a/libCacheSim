//
// Created by Juncheng Yang on 12/10/19.
//

#include "slabObjLRU.h"

//
// Created by Juncheng Yang on 11/22/19.
//

#include "slabObjLRU.h"


#ifdef __cplusplus
extern "C" {
#endif

void _slabObjLRU_insert(cache_t *slabObjLRU, request_t *req) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (slabObjLRU->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = key;

  g_queue_push_tail_link(slabObjLRU_params->list, node);
  g_hash_table_insert(slabObjLRU_params->hashtable, (gpointer) key, (gpointer) node);
}

gboolean slabObjLRU_check(cache_t *cache, request_t *req) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);
  return g_hash_table_contains(slabObjLRU_params->hashtable, req->obj_id_ptr);
}

void _slabObjLRU_update(cache_t *cache, request_t *req) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);
  GList *node =
    (GList *) g_hash_table_lookup(slabObjLRU_params->hashtable, req->obj_id_ptr);
  g_queue_unlink(slabObjLRU_params->list, node);
  g_queue_push_tail_link(slabObjLRU_params->list, node);
}

void _slabObjLRU_evict(cache_t *slabObjLRU, request_t *req) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (slabObjLRU->cache_params);
  gpointer data = g_queue_pop_head(slabObjLRU_params->list);
  g_hash_table_remove(slabObjLRU_params->hashtable, (gconstpointer) data);

}

gpointer _slabObjLRU_evict_with_return(cache_t *slabObjLRU, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (slabObjLRU->cache_params);

  gpointer data = g_queue_pop_head(slabObjLRU_params->list);

  gpointer evicted_key = data;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) data);
  }

  g_hash_table_remove(slabObjLRU_params->hashtable, (gconstpointer) data);
  return evicted_key;
}

gboolean slabObjLRU_add(cache_t *cache, request_t *req) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);
  gboolean retval;
  if (slabObjLRU_check(cache, req)) {
    _slabObjLRU_update(cache, req);
    slabObjLRU_params->ts++;
    retval = TRUE;
  } else {
    _slabObjLRU_insert(cache, req);
    if ((long) g_hash_table_size(slabObjLRU_params->hashtable) > cache->core->size)
      _slabObjLRU_evict(cache, req);
    slabObjLRU_params->ts++;
    retval = FALSE;
  }
  return retval;
}


void slabObjLRU_destroy(cache_t *cache) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);

  //    g_queue_free(slabObjLRU_params->list);                 // Jason: should call
  //    g_queue_free_full to free the memory of node content
  // 0921
  g_queue_free(slabObjLRU_params->list);
  g_hash_table_destroy(slabObjLRU_params->hashtable);
  cache_struct_free(cache);
}

void slabObjLRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  slabObjLRU_destroy(cache);
}

/*-----------------------------------------------------------------------------
 *
 * slabObjLRU_init --
 *      initialize a slabObjLRU cacheAlgo
 *
 * Input:
 *      size:       cacheAlgo size
 *      obj_id_type:  the obj_id_type of data, currently support l for long or c
 *for string block_size: the basic unit size of block, used for profiling with
 *size if not evaluate with size, this is 0 params:     params used for
 *initialization, NULL for slabObjLRU
 *
 * Return:
 *      a slabObjLRU cacheAlgo struct
 *
 *-----------------------------------------------------------------------------
 */
cache_t *slabObjLRU_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("slabObjLRU", size, obj_id_type);
  cache->cache_params = g_new0(struct slabObjLRU_params, 1);
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);

//  cacheAlgo->core->cache_init = slabObjLRU_init;
//  cacheAlgo->core->destroy = slabObjLRU_destroy;
//  cacheAlgo->core->destroy_unique = slabObjLRU_destroy_unique;
//  cacheAlgo->core->add = slabObjLRU_add;
//  cacheAlgo->core->check = slabObjLRU_check;
//  cacheAlgo->core->_insert = _slabObjLRU_insert;
//  cacheAlgo->core->_update = _slabObjLRU_update;
//  cacheAlgo->core->_evict = _slabObjLRU_evict;
//  cacheAlgo->core->evict_with_return = _slabObjLRU_evict_with_return;
//  cacheAlgo->core->get_used_size = slabObjLRU_get_size;
//  cacheAlgo->core->remove_obj = slabObjLRU_remove_obj;
//  cacheAlgo->core->cache_init_params = NULL;
//  cacheAlgo->core->add_only = slabObjLRU_add_only;
//  cacheAlgo->core->add_withsize = slabObjLRU_add_withsize;

  if (obj_id_type == OBJ_ID_NUM) {
    slabObjLRU_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    slabObjLRU_params->hashtable = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  slabObjLRU_params->list = g_queue_new();

  return cache;
}

void slabObjLRU_remove_obj(cache_t *cache, void *data_to_remove) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);

  gpointer data = g_hash_table_lookup(slabObjLRU_params->hashtable, data_to_remove);
  if (!data) {
    fprintf(stderr, "slabObjLRU_remove_obj: data to remove is not in the cacheAlgo\n");
    exit(1);
  }
  g_queue_delete_link(slabObjLRU_params->list, (GList *) data);
  g_hash_table_remove(slabObjLRU_params->hashtable, data_to_remove);
}

guint64 slabObjLRU_get_current_size(cache_t *cache) {
  struct slabObjLRU_params *slabObjLRU_params = (struct slabObjLRU_params *) (cache->cache_params);
  return (guint64) g_hash_table_size(slabObjLRU_params->hashtable);
}

#ifdef __cplusplus
}
#endif

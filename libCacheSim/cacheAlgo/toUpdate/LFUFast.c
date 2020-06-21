////
////  LFUFast.c
////  libCacheSim
////
////  Created by Juncheng on 6/2/16.
////  Copyright © 2016 Juncheng. All rights reserved.
////
//
///* this module uses linkedlist to order requests,
// * which is O(1) at each request, which should be preferred in most cases
// * the drawback of this implementation is the memory usage, because two pointers
// * are associated with each obj_id
// *
// * this LFUFast clear the frequency of an obj_id after evicting from cache
// * this LFUFast is LFU_LFUFast, which choose LFUFast when more than one items have
// * the same smallest freq
// */
//
//
//#ifdef __cplusplus
//extern "C" {
//#endif
//
//#include "LFUFast.h"
//
//
//
///********************************** internal ************************************/
//gboolean _LFUFast_verify(cache_t *LFUFast) {
//
//  LFUFast_params_t *LFUFast_params =
//    (LFUFast_params_t *) (LFUFast->cache_params);
//  GList *mnode_list = g_queue_peek_head_link(LFUFast_params->main_list);
//  main_list_node_data_t *mnode_data;
//  //    branch_list_node_data_t *bnode_data;
//  guint64 current_size = 0;
//  while (mnode_list) {
//    mnode_data = mnode_list->data;
//    current_size += g_queue_get_length(mnode_data->queue);
//    printf("%u\t", g_queue_get_length(mnode_data->queue));
//    mnode_list = mnode_list->next;
//  }
//  printf("\n");
//  if (g_hash_table_size(LFUFast_params->hashtable) == current_size)
//    return TRUE;
//  else {
//    ERROR("hashtable size %u, queue accu size %lu\n",
//          g_hash_table_size(LFUFast_params->hashtable),
//          (unsigned long) current_size);
//    return FALSE;
//  }
//}
//
//void free_main_list_node_data(gpointer data) {
//  main_list_node_data_t *mnode_data = data;
//  g_queue_free_full(mnode_data->queue, g_free);
//  g_free(data);
//}
//
///********************************** LFUFast ************************************/
//cache_t *LFUFast_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
//  cache_t *cache = cache_struct_init("LFUFast", ccache_params);
//  cache->cache_params = g_new0(LFUFast_params_t, 1);
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  LFUFast_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, NULL, g_free, NULL);
//
//  LFUFast_params->min_freq = 0;
//  LFUFast_params->main_list = g_queue_new();
//  return cache;
//}
//
//void LFUFast_free(cache_t *cache) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  g_hash_table_destroy(LFUFast_params->hashtable);
//  g_queue_free_full(LFUFast_params->main_list, free_main_list_node_data);
//  cache_struct_free(cache);
//}
//
//gboolean LFUFast_check(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  return g_hash_table_contains(LFUFast_params->hashtable, req->obj_id_ptr);
//}
//
//
//gboolean LFUFast_get(cache_t *cache, request_t *req) {
//  gboolean found_in_cache = LFUFast_check(cache, req);
//  if (req->obj_size <= cache->size) {
//    if (found_in_cache)
//      _LFUFast_update(cache, req);
//    else
//      _LFUFast_insert(cache, req);
//
//    while (cache->used_size > cache->size)
//      _LFUFast_evict(cache, req);
//  } else {
//    WARNING("req %lld: obj size %ld larger than cache size %ld\n", (long long) cache->req_cnt,
//            (long) req->obj_size, (long) cache->size);
//  }
//  cache->req_cnt += 1;
//  return found_in_cache;
//}
//
//void _LFUFast_insert(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  cache->used_size += req->obj_size;
//  cache_obj_t *cache_obj = create_cache_obj_from_request(req);
//  GList *node = g_list_alloc();
//  node->data = cache_obj;
//  g_queue_push_tail_link(LFUFast_params->list, node);
//  g_hash_table_insert(LFUFast_params->hashtable, cache_obj->obj_id_ptr, (gpointer) node);
//}
//
//void _LFUFast_update(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  GList *node = (GList *) g_hash_table_lookup(LFUFast_params->hashtable, req->obj_id_ptr);
//
//  cache_obj_t *cache_obj = node->data;
//  assert(cache->used_size >= cache_obj->obj_size);
//  // there is potential bug that if obj size is larger than cache size
//  if (cache_obj->obj_size != req->obj_size){
//    cache->used_size -= cache_obj->obj_size;
//    cache->used_size += req->obj_size;
//    cache_obj->obj_size = req->obj_size;
//  }
//  g_queue_unlink(LFUFast_params->list, node);
//  g_queue_push_tail_link(LFUFast_params->list, node);
//}
//
//cache_obj_t *LFUFast_get_cached_obj(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  GList *node = (GList *) g_hash_table_lookup(LFUFast_params->hashtable, req->obj_id_ptr);
//  cache_obj_t *cache_obj = node->data;
//  return cache_obj;
//}
//
//void _LFUFast_evict(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (cache->cache_params);
//  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(LFUFast_params->list);
//  assert(cache->used_size >= cache_obj->obj_size);
//  cache->used_size -= cache_obj->obj_size;
//  g_hash_table_remove(LFUFast_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
//  destroy_cache_obj(cache_obj);
//}
//
//
//
//
//
//
//
//
//
//void _LFUFast_insert(cache_t *LFUFast, request_t *req) {
//  LFUFast_params_t *LFUFast_params = (LFUFast_params_t *) (LFUFast->cache_params);
//
//  gpointer key = req->obj_id_ptr;
//  if (req->obj_id_type == OBJ_ID_STR) {
//    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
//  }
//  branch_list_node_data_t *bnode_data = g_new0(branch_list_node_data_t, 1);
//  bnode_data->key = key;
//  GList *list_node = g_list_append(NULL, bnode_data);
//
//  g_hash_table_insert(LFUFast_params->hashtable, (gpointer) key,
//                      (gpointer) list_node);
//
//  main_list_node_data_t *mnode_data;
//  if (LFUFast_params->min_freq != 1) {
//    // initial
//    if (LFUFast_params->min_freq < 1 &&
//        LFUFast_params->main_list->length != 0) {
//      WARNING("LFU initialization error\n");
//    }
//    mnode_data = g_new0(main_list_node_data_t, 1);
//    mnode_data->freq = 1;
//    mnode_data->queue = g_queue_new();
//    g_queue_push_head(LFUFast_params->main_list, mnode_data);
//    LFUFast_params->min_freq = 1;
//  } else {
//    mnode_data = (main_list_node_data_t *) (g_queue_peek_head(
//      LFUFast_params->main_list));
//#ifdef SANITY_CHECK
//    if (mnode_data->freq != 1) {
//      ERROR("first main node freq is not 1, is %d\n", mnode_data->freq);
//      exit(1);
//    }
//#endif
//  }
//
//  g_queue_push_tail_link(mnode_data->queue, list_node);
//  bnode_data->main_list_node =
//    g_queue_peek_head_link(LFUFast_params->main_list);
//}
//
//void _LFUFast_update(cache_t *cache, request_t *req) {
//
//
//  GList *list_node = g_hash_table_lookup(LFUFast_params->hashtable,
//                                         (gconstpointer) (req->obj_id_ptr));
//
//  branch_list_node_data_t *bnode_data = list_node->data;
//  main_list_node_data_t *mnode_data = bnode_data->main_list_node->data;
//
//  // remove from current main_list_node
//  g_queue_unlink(mnode_data->queue, list_node);
//
//  // check whether there is next
//  gboolean exist_next =
//    (bnode_data->main_list_node->next == NULL) ? FALSE : TRUE;
//
//  // check whether next is freq+1
//  main_list_node_data_t *mnode_next_data =
//    exist_next ? bnode_data->main_list_node->next->data : NULL;
//  if (exist_next && mnode_next_data->freq == mnode_data->freq + 1) {
//    // insert to this main list node
//    g_queue_push_tail_link(mnode_next_data->queue, list_node);
//  } else {
//#ifdef SANITY_CHECK
//    if (exist_next && mnode_next_data->freq <= mnode_data->freq) {
//      ERROR("mnode next freq %d, current freq %d\n", mnode_next_data->freq,
//            mnode_data->freq);
//      exit(-1);
//    }
//#endif
//    // create a new main list node, insert in between
//    main_list_node_data_t *new_mnode_data = g_new0(main_list_node_data_t, 1);
//    new_mnode_data->freq = mnode_data->freq + 1;
//    new_mnode_data->queue = g_queue_new();
//    g_queue_push_tail_link(new_mnode_data->queue, list_node);
//    // insert mnode
//    g_queue_insert_after(LFUFast_params->main_list, bnode_data->main_list_node,
//                         new_mnode_data);
//  }
//  bnode_data->main_list_node = bnode_data->main_list_node->next;
//}
//
//void _LFUFast_evict(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params =
//    (LFUFast_params_t *) (cache->cache_params);
//
//  GList *mnode = g_queue_peek_head_link(LFUFast_params->main_list);
//  main_list_node_data_t *mnode_data =
//    g_queue_peek_head(LFUFast_params->main_list);
//
//  // find the first main list node that has an non-empty queue
//  while (g_queue_is_empty(mnode_data->queue)) {
//    mnode = mnode->next;
//    mnode_data = mnode->data;
//  }
//  branch_list_node_data_t *bnode_data = g_queue_pop_head(mnode_data->queue);
//  g_hash_table_remove(LFUFast_params->hashtable,
//                      (gconstpointer) (bnode_data->key));
//
//  g_free(bnode_data);
//}
//
//
//gboolean LFUFast_add(cache_t *cache, request_t *req) {
//  LFUFast_params_t *LFUFast_params =
//    (LFUFast_params_t *) (cache->cache_params);
//  gboolean retval;
//
//  if (LFUFast_check(cache, req)) {
//    _LFUFast_update(cache, req);
//    retval = TRUE;
//  } else {
//    _LFUFast_insert(cache, req);
//    if ((long) g_hash_table_size(LFUFast_params->hashtable) > cache->size)
//      _LFUFast_evict(cache, req);
//    retval = FALSE;
//  }
//  cache->req_cnt += 1;
//  return retval;
//}
//
//
////void LFUFast_remove_obj(cache_t *cache, void *data_to_remove) {
////  LFUFast_params_t *LFUFast_params =
////    (LFUFast_params_t *) (cache->cache_params);
////  GList *blist_node =
////    g_hash_table_lookup(LFUFast_params->hashtable, data_to_remove);
////
////  branch_list_node_data_t *bnode_data = blist_node->data;
////  main_list_node_data_t *mnode_data = bnode_data->main_list_node->data;
////  g_queue_unlink(mnode_data->queue, blist_node);
////
////  g_hash_table_remove(LFUFast_params->hashtable,
////                      (gconstpointer) (data_to_remove));
////}
//
//#ifdef __cplusplus
//}
//#endif

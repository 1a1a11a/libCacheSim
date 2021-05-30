//
// Created by Juncheng Yang on 3/19/20.
//

#ifndef libCacheSim_DISTUTILSINTERNAL_H
#define libCacheSim_DISTUTILSINTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../dataStructure/include/splay.h"
#include "../../utils/include/utilsInternal.h"
//#include "../../include/libCacheSim.h"

/***********************************************************
 * this function is called by _get_last_access_dist,
 * it insert current request and return distance to its last request
 * note that the distance between req at t and at t+1 is 0,
 * the calculated dist = cur_ts - last_ts - 1
 *
 * @param req           request_t contains current request
 * @param hash_table    the hashtable for remembering last access
 * @param ts            current timestamp
 * @return              distance to last access
 */
static inline gint64
_get_last_dist_add_req(request_t *req, GHashTable *hash_table, guint64 ts) {
  gpointer gp =
      g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int));
  gint64 ret = -1;
  if (gp == NULL) {
    // it has not been requested before
    ;
  } else {
    // it has been requested before
    gsize old_ts = GPOINTER_TO_SIZE(gp);
    ret = (gint64)ts - (gint64)old_ts - 1;
  }
  g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id_int),
                      GSIZE_TO_POINTER((gsize)ts));
  return ret;
}

/***********************************************************
 * this function is called by _get_first_access_dist,
 * it inserts current request and return distance to its first request
 * note that the distance between req at t and at t+1 is 0,
 * the calculated dist = cur_ts - last_ts - 1
 *
 * @param req           request_t contains current request
 * @param hash_table    the hashtable for remembering last access
 * @param ts            current timestamp
 * @return              distance to last access
 */
static inline gint64
_get_first_dist_add_req(request_t *req, GHashTable *hash_table, guint64 ts) {
  gpointer gp =
      g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int));
  gint64 ret = -1;
  if (gp == NULL) {
    // it has not been requested before
    g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id_int),
                        GSIZE_TO_POINTER((gsize)ts));
  } else {
    // it has been requested before
    gsize old_ts = GPOINTER_TO_SIZE(gp);
    ret = (gint64)ts - (gint64)old_ts - 1;
  }

  return ret;
}

/***********************************************************
 * this function is used for computing reuse distance for each request
 * it maintains a hashmap and a splay tree,
 * time complexity is O(log(N)), N is the number of unique elements
 * @param req               the request struct contains input data (request
 * obj_id)
 * @param splay_tree        the splay tree struct
 * @param hash_table        hashtable for remember last request timestamp
 * @param ts                current timestamp
 * @param stack_dist        the calculated reuse distance (return value)
 * @return                  pointer to the new splay tree, because splay tree is
 * modified every time, so it is essential to update the splay tree
 */
static inline sTree *get_stack_dist_add_req(request_t *req, sTree *splay_tree,
                                            GHashTable *hash_table, guint64 ts,
                                            gint64 *stack_dist) {

  gpointer gp =
      g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int));

  sTree *newtree;
  if (gp == NULL) {
    // first time access
    *stack_dist = -1;
    newtree = insert(ts, splay_tree);
  } else {
    // not first time access
    gint64 old_ts = (gint64)GPOINTER_TO_SIZE(gp);
    newtree = splay(old_ts, splay_tree);
    *stack_dist = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);
  }

  g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id_int),
                      (gpointer)GSIZE_TO_POINTER((gsize)ts));
  return newtree;
}

/*****************************************************
 * not working functions
 *
 *
 *
 *
 *
 * @param req
 * @param splay_tree
 * @param hash_table
 * @param ts
 * @param byte_stack_dist
 * @return
 */
static inline sTree *get_byte_stack_dist_add_req(request_t *req,
                                                 sTree *splay_tree,
                                                 GHashTable *hash_table,
                                                 guint64 ts,
                                                 gint64 *byte_stack_dist) {
  gpointer gp =
      g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int));

  sTree *newtree;
  if (gp == NULL) {
    // first time access
    *byte_stack_dist = -1;
    newtree = insert(ts, splay_tree);
  } else {
    // not first time access
    gint64 old_ts = (gint64)GPOINTER_TO_SIZE(gp);
    newtree = splay(old_ts, splay_tree);
    *byte_stack_dist = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);
  }

  g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id_int),
                      (gpointer)GSIZE_TO_POINTER((gsize)ts));
  return newtree;
}

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_DISTUTILSINTERNAL_H

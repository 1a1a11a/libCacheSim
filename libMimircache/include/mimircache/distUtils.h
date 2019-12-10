//
// Created by Juncheng Yang on 11/24/19.
//

#ifndef MIMIRCACHE_DISTUTILS_H
#define MIMIRCACHE_DISTUTILS_H

#include "reader.h"
#include "csvReader.h"
#include "utils.h"
#include "const.h"
#include "splay.h"

/**
 * sequential version of get_reuse_dist
 * @param reader
 * @return
 */
gint64 *_get_reuse_dist_seq(reader_t *reader);

/**
 * get the reuse distance (stack distance) since last access,
 * it returns an array of n_req, where the nth element is the
 * reuse distance (number of unique obj) since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_dist(reader_t *reader);

/**
 * get the future reuse distance (stack distance) since now,
 * it returns an array of n_req, where the nth element is the
 * future reuse distance (number of unique obj) from now to next access for this obj
 * @param reader
 * @return
 */
gint64 *get_future_reuse_dist(reader_t *reader);


/**
 * get the resue time since last access,
 * it returns an array of n_req, where the nth element is the
 * time since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_time(reader_t *reader);

/**
 * get the distance since last access,
 * it returns an array of n_req, where the nth element is the
 * distance (number of obj) since last access for this object
 * @param reader
 * @return
 */
gint64 *get_last_access_dist(reader_t *reader);

/**
 * get the dist to next access,
 * it returns an array of n_req, where the nth element is the
 * distance (number of obj) since now to next access for this obj
 * note: it is better to rewrite this function to not use read_one_req_above, which has high overhead
 * @param reader
 * @return
 */
gint64 *get_next_access_dist(reader_t *reader);


/**
 * get the reuse byte (reuse distance in bytes) since last access,
 * it returns an array of n_req, where the nth element is the
 * reuse byte (number of unique obj) since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_byte(reader_t *reader);

gint64 *get_last_access_byte(reader_t *reader);


/*-----------------------------------------------------------------------------
 *
 * cal_save_reuse_dist --
 *      compute reuse distance or future reuse distance,
 *      then save to file to facilitate future computation
 *
 *      In detail, this function calculates reuse distance,
 *          then saves the array to the specified location,
 *          for each entry in the array, saves using gint64
 *
 * Input:
 *      reader:         the reader for data
 *      save_file_loc   the location to save frd file
 *
 * Return:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
gint64 *cal_save_reuse_dist(reader_t *const reader,
                            const char *const save_file_loc,
                            const dist_t dist_type);


/*-----------------------------------------------------------------------------
 *
 * load_reuse_dist --
 *      this function is used for loading either reuse distance or
 *      future reuse distance from the file, which is pre-computed
 *
 *
 * Input:
 *      reader:         the reader for saving data
 *      load_file_loc   the location to file for loading REUSE_DIST or frd
 *
 * Return:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
gint64 *load_reuse_dist(reader_t *const reader,
                        const char *const load_file_loc,
                        const dist_t dist_type);


/*-----------------------------------------------------------------------------
 *
 * _get_last_dist_add_req --
 *      this function is called by _get_last_access_dist_seq,
 *      it insert current request and return distance to its last request
 *
 * Potential bug:
 *      No
 *
 * Input:
 *      req:             request_t contains current request
 *      hash_table:     the hashtable for remembering last access
 *      ts:             current timestamp
 *
 *
 * Return:
 *      distance to last request
 *
 *-----------------------------------------------------------------------------
 */

static inline gint64 _get_last_dist_add_req(request_t *req, GHashTable *hash_table, guint64 ts) {
  gpointer gp = g_hash_table_lookup(hash_table, req->obj_id_ptr);
  gint64 ret = -1;
  if (gp == NULL) {
    // it has not been requested before
    ;
  } else {
    // it has been requested before
    gsize old_ts = GPOINTER_TO_SIZE(gp);
    ret = (gint64) ts - (gint64) old_ts;
  }
  if (req->obj_id_type == OBJ_ID_STR)
    g_hash_table_insert(hash_table, g_strdup((gchar *) (req->obj_id_ptr)), GSIZE_TO_POINTER((gsize) ts));
  else if (req->obj_id_type == OBJ_ID_NUM) {
    g_hash_table_insert(hash_table, req->obj_id_ptr, GSIZE_TO_POINTER((gsize) ts));
  } else {
    ERROR("unknown obj_id_type: %c\n", req->obj_id_type);
    exit(1);
  }

  return ret;
}


/*-----------------------------------------------------------------------------
 *
 * get_reuse_dist_add_req --
 *      this function is used for computing reuse distance for each request
 *      it maintains a hashmap and a splay tree,
 *      time complexity is O(log(N)), N is the number of unique elements
 *
 *
 * Input:
 *      req           the request struct contains input data (request obj_id)
 *      splay_tree   the splay tree struct
 *      hash_table   hashtable for remember last request timestamp (virtual)
 *      ts           current timestamp
 *      REUSE_DIST   the calculated reuse distance
 *
 * Return:
 *      splay tree struct pointer, because splay tree is modified every time,
 *      so it is essential to update the splay tree
 *
 *-----------------------------------------------------------------------------
 */
static inline sTree *get_reuse_dist_add_req(request_t *req,
                                            sTree *splay_tree,
                                            GHashTable *hash_table,
                                            guint64 ts,
                                            gint64 *reuse_dist) {
  gpointer gp = g_hash_table_lookup(hash_table, req->obj_id_ptr);

  sTree *newtree;
  if (gp == NULL) {
    // first time access
    *reuse_dist = -1;
    newtree = insert(ts, splay_tree);
  } else {
    // not first time access
    gint64 old_ts = (gint64) GPOINTER_TO_SIZE(gp);
    newtree = splay(old_ts, splay_tree);
    *reuse_dist = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);

  }

  if (req->obj_id_type == OBJ_ID_STR)
    g_hash_table_insert(hash_table, g_strdup((gchar *) (req->obj_id_ptr)), GSIZE_TO_POINTER((gsize) ts));
  else if (req->obj_id_type == OBJ_ID_NUM) {
    g_hash_table_insert(hash_table, req->obj_id_ptr, (gpointer) GSIZE_TO_POINTER((gsize) ts));
  } else {
    ERROR("unknown request obj_id_type: %c\n", req->obj_id_type);
    abort();
  }
  return newtree;
}


static inline sTree *get_byte_reuse_dist_add_req(request_t *req,
                                            sTree *splay_tree,
                                            GHashTable *hash_table,
                                            guint64 ts,
                                            gint64 *byte_reuse_dist) {
  gpointer gp = g_hash_table_lookup(hash_table, req->obj_id_ptr);

  sTree *newtree;
  if (gp == NULL) {
    // first time access
    *byte_reuse_dist = -1;
    newtree = insert(ts, splay_tree);
  } else {
    // not first time access
    gint64 old_ts = (gint64) GPOINTER_TO_SIZE(gp);
    newtree = splay(old_ts, splay_tree);
    *byte_reuse_dist = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);

  }

  if (req->obj_id_type == OBJ_ID_STR)
    g_hash_table_insert(hash_table, g_strdup((gchar *) (req->obj_id_ptr)), GSIZE_TO_POINTER((gsize) ts));
  else if (req->obj_id_type == OBJ_ID_NUM) {
    g_hash_table_insert(hash_table, req->obj_id_ptr, (gpointer) GSIZE_TO_POINTER((gsize) ts));
  } else {
    ERROR("unknown request obj_id_type: %c\n", req->obj_id_type);
    abort();
  }
  return newtree;
}

/*-----------------------------------------------------------------------------
 *
 * get_reuse_dist_add_req_shards--
 *      this function is used for computing reuse distance for each request that
 *      satisfies the shards conditions
 *      it maintains a hashmap and a splay tree,
 *      time complexity is O(log(N)), N is the number of unique elements
 *
 *
 * Input:
 *      req           the request struct contains input data (request obj_id)
 *      splay_tree   the splay tree struct
 *      hash_table   hashtable for remember last request timestamp (virtual)
 *      ts           current timestamp
 *      REUSE_DIST   the calculated reuse distance
 *      sample_ratio the sample ratio for sharding
 *
 * Return:
 *      splay tree struct pointer, because splay tree is modified every time,
 *      so it is essential to update the splay tree
 *
 *-----------------------------------------------------------------------------
 */
static inline sTree *get_reuse_dist_add_req_shards(request_t *req,
                                                   sTree *splay_tree,
                                                   GHashTable *hash_table,
                                                   guint64 ts,
                                                   gint64 *reuse_dist, double sample_ratio) {
  gpointer gp;

  gp = g_hash_table_lookup(hash_table, req->obj_id_ptr);

  sTree *newtree;
  if (gp == NULL) {
// first time access
    newtree = insert(ts, splay_tree);
    gint64 *value = g_new(gint64, 1);
    if (value == NULL) {
      ERROR("not enough memory\n");
      exit(1);
    }
    *value = ts;
    if (req->obj_id_type == OBJ_ID_STR)
      g_hash_table_insert(hash_table, g_strdup((gchar *) (req->obj_id_ptr)), (gpointer) value);

    else if (req->obj_id_type == OBJ_ID_NUM) {
      gint64 *key = g_new(gint64, 1);
      *key = *(guint64 *) (req->obj_id_ptr);
      g_hash_table_insert(hash_table, (gpointer) (key), (gpointer) value);
    } else {
      ERROR("unknown request content obj_id_type: %c\n", req->obj_id_type);
      exit(1);
    }
    *reuse_dist = -1;
  } else {
// not first time access
    guint64 old_ts = *(guint64 *) gp;
    newtree = splay(old_ts, splay_tree);
// rescaling the histrograms should I be diving the ts by sample ratio?
// because right now I am not storing the scaled values but scaline them
// once I retrieve it.
    *reuse_dist = node_value(newtree->right);
    *(guint64 *) gp = ts;
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);

  }
  return newtree;
}


#endif //MIMIRCACHE_DISTUTILS_H

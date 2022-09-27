//
//  eviction_stat.c
//  libCacheSim
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "eviction_stat.h"


#include "../../include/libCacheSim/reader.h"
#include "../../cacheAlg/dep/FIFO.h"
#include "Belady.h"
#include "LRU_K.h"
#include "../../cacheAlg/dep/LRU.h"

#ifdef __cplusplus
extern "C"
{
#endif


static gint64 *get_eviction_stack_dist(reader_t *reader, cache_t *optimal);

static gint64 *get_eviction_freq(reader_t *reader, cache_t *optimal, gboolean accumulative);

//static gdouble* get_eviction_relative_freq(READER* reader, cache_t* optimal);
static inline sTree *
process_one_element_eviction_stack_dist(request_t *req, sTree *splay_tree, GHashTable *hash_table, guint64 ts,
                                        gint64 *stack_dist, gpointer evicted);


static void traverse_trace(reader_t *reader, cache_t *cache) {
  /** this function traverse the trace file, add each request to cache, just like a cache simulation 
   *
   **/

  // create request struct and initialization
  request_t *req = new_req_struct(reader->base->label_type);

  req->label_type = cache->data_type;
  req->block_unit_size = (size_t) reader->base->block_unit_size;

  gboolean (*add_element)(struct cache *, request_t *req);
  add_element = cache->add_element;

  read_one_element(reader, req);
  while (req->valid) {
    add_element(cache, req);
    read_one_element(reader, req);
  }

  // clean up
  free_request(req);
  reset_reader(reader);

}


gint64 *eviction_stat(reader_t *reader_in, cache_t *cache, evict_stat_type stat_type) {
  /** this function first traverse the trace file to generate a list of evicted requests,
   ** then again traverse the trace file to obtain the statistics of evicted requests 
   **/

  // get cache eviction list
  cache->record_level = 1;
  cache->eviction_array_len = reader_in->base->total_num;
  if (reader_in->base->total_num == -1)
    get_num_of_req(reader_in);

  if (reader_in->base->obj_id_type == OBJ_ID_NUM)
    cache->eviction_array = g_new0(guint64, reader_in->base->total_num);
  else
    cache->eviction_array = g_new0(gchar*, reader_in->base->total_num);

  traverse_trace(reader_in, cache);
  // done get eviction list


  if (stat_type == evict_stack_dist) {
    return get_eviction_stack_dist(reader_in, cache);
  } else if (stat_type == evict_freq) {
    return get_eviction_freq(reader_in, cache, FALSE);
  } else if (stat_type == evict_freq_accumulatve) {
    return get_eviction_freq(reader_in, cache, TRUE);
  } else if (stat_type == evict_data_classification) {
    return NULL;
  } else {
    ERROR("unsupported stat label_type\n");
    exit(1);
  }
}


gdouble *
eviction_stat_over_time(reader_t *reader_in, char mode, guint64 time_interval, guint64 cache_size, char *stat_type) {

  if (mode == 'r')
    get_bp_rtime(reader_in, time_interval, -1);
  else
    get_bp_vtime(reader_in, time_interval, -1);


  return NULL;
}


gint64 *get_eviction_freq(reader_t *reader, cache_t *optimal, gboolean accumulative) {
  /** if insert then evict, its freq should be 1,
      in other words, the smallest freq should be 1, 
      if there is no eviction at ts, then it is -1.
   **/

  guint64 ts = 0;

  gpointer eviction_array = optimal->eviction_array;

  gint64 *freq_array = g_new0(gint64, reader->base->total_num);


  // create request struct and initializa
  request_t *req = new_req_struct(reader->base->label_type);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_int64_hash, g_int64_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else {
    ERROR("does not recognize reader data label_type %c\n", reader->base->label_type);
    abort();
  }

  gpointer gp;
  read_one_element(reader, req);

  while (req->valid) {
    gp = g_hash_table_lookup(hash_table, req->label_ptr);
    if (gp == NULL) {
      // first time access
      gint64 *value = g_new(gint64, 1);
      *value = 1;
      if (req->obj_id_type == OBJ_ID_STR) {
        g_hash_table_insert(hash_table, g_strdup((gchar *) (req->label_ptr)), (gpointer) (value));
      } else if (req->obj_id_type == OBJ_ID_NUM) {
        gint64 *key = g_new(gint64, 1);
        *key = *(guint64 *) (req->label_ptr);
        g_hash_table_insert(hash_table, (gpointer) (key), (gpointer) (value));
      } else {
        printf("unknown request content label_type: %c\n", req->label_type);
        exit(1);
      }
    } else {
      *(gint64 *) gp = *(gint64 *) gp + 1;
    }
    read_one_element(reader, req);


    // get freq of evicted label
    if (req->obj_id_type == OBJ_ID_STR) {
      if (((gchar **) eviction_array)[ts] == NULL)
        freq_array[ts++] = -1;
      else {
        gp = g_hash_table_lookup(hash_table, ((gchar **) eviction_array)[ts]);
        freq_array[ts++] = *(gint64 *) gp;

        // below line should be enabled only when we want to clear the freq count after eviction
        if (!accumulative)
          *(gint64 *) gp = 0;
      }
    } else if (req->obj_id_type == OBJ_ID_NUM) {
      if (*((guint64 *) eviction_array + ts) == 0)
        freq_array[ts++] = -1;
      else {
        gp = g_hash_table_lookup(hash_table, ((guint64 *) eviction_array + ts));
        freq_array[ts++] = *(gint64 *) gp;

        // below line should be enabled only when we want to clear the freq count after eviction
        if (!accumulative)
          *(gint64 *) gp = 0;
      }
    }
  }

  destroy_req_struct(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return freq_array;
}


static gint64 *get_eviction_stack_dist(reader_t *reader, cache_t *optimal) {
  /*
   * TODO: might be better to split return result, in case the hit rate array is too large
   * Is there a better way to do this? this will cause huge amount memory
   * It is the user's responsibility to release the memory of hit count array returned by this function
   */

  guint64 ts = 0;
  gint64 stack_dist;

  gpointer eviction_array = optimal->eviction_array;

  gint64 *stack_dist_array = g_new0(gint64, reader->base->total_num);


  // create request struct and initializa
  request_t *req = new_req_struct(reader->base->label_type);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    //        req->obj_id_type == OBJ_ID_NUM;
    hash_table = g_hash_table_new_full(g_int64_hash, g_int64_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    //        req->obj_id_type == OBJ_ID_STR;
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else {
    ERROR("does not recognize reader data label_type %c\n", reader->base->label_type);
    abort();
  }

  // create splay tree
  sTree *splay_tree = NULL;


  read_one_element(reader, req);

  if (req->obj_id_type == OBJ_ID_NUM) {
    while (req->valid) {
      splay_tree = process_one_element_eviction_stack_dist(req, splay_tree, hash_table, ts, &stack_dist,
                                                           (gpointer) ((guint64 *) eviction_array + ts));
      stack_dist_array[ts] = stack_dist;
      read_one_element(reader, req);
      ts++;
    }
  } else {
    while (req->valid) {
      splay_tree = process_one_element_eviction_stack_dist(req, splay_tree, hash_table, ts, &stack_dist,
                                                           (gpointer) (((gchar **) eviction_array)[ts]));
      stack_dist_array[ts] = stack_dist;
      read_one_element(reader, req);
      ts++;
    }
  }


  // clean up
  destroy_req_struct(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return stack_dist_array;
}


static inline sTree *
process_one_element_eviction_stack_dist(request_t *req, sTree *splay_tree, GHashTable *hash_table, guint64 ts,
                                        gint64 *stack_dist, gpointer evicted) {
  gpointer gp;

  gp = g_hash_table_lookup(hash_table, req->label_ptr);

  sTree *newtree = splay_tree;
  if (gp == NULL) {
    // first time access
    newtree = insert(ts, splay_tree);
    gint64 *value = g_new(gint64, 1);
    *value = ts;
    if (req->obj_id_type == OBJ_ID_STR)
      g_hash_table_insert(hash_table, g_strdup((gchar *) (req->label_ptr)), (gpointer) value);

    else if (req->obj_id_type == OBJ_ID_NUM) {
      gint64 *key = g_new(gint64, 1);
      *key = *(guint64 *) (req->label_ptr);
      g_hash_table_insert(hash_table, (gpointer) (key), (gpointer) value);
    } else {
      printf("unknown request content label_type: %c\n", req->label_type);
      exit(1);
    }
  } else {
    // not first time access
    // save old ts, update ts in hashtable
    guint64 old_ts = *(guint64 *) gp;
    *(guint64 *) gp = ts;

    // update splay tree
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(ts, newtree);

  }

  // get evicted reuse distance
  if (evicted) {
    if (req->obj_id_type == OBJ_ID_NUM)
      if (*(guint64 *) evicted == 0) {
        *stack_dist = -1;
        return newtree;
      }
    gp = g_hash_table_lookup(hash_table, evicted);
    guint64 old_ts = *(guint64 *) gp;
    newtree = splay(old_ts, newtree);
    *stack_dist = node_value(newtree->right);
  } else
    *stack_dist = -1;


  return newtree;
}


#ifdef __cplusplus
}
#endif

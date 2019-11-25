//
// Created by Juncheng Yang on 11/24/19.
//

#include "../include/mimircache/distUtils.h"


gint64 *_get_reuse_dist_seq(reader_t *reader) {
  // check whether the reuse LAST_DIST computation has been finished
  if (reader->sdata->reuse_dist &&
      reader->sdata->reuse_dist_type == REUSE_DIST) {
    return reader->sdata->reuse_dist;
  }

  guint64 ts = 0;
  gint64 max_rd = 0;
  gint64 rd = 0;
  get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);

  gint64 *reuse_dist_array = g_new(gint64, reader->base->n_total_req);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);
  } else {
    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }

  // create splay tree
  sTree *splay_tree = NULL;

  read_one_req(reader, req);
  while (req->valid) {
    splay_tree = get_reuse_dist_add_req(req, splay_tree, hash_table, ts, &rd);
    reuse_dist_array[ts] = rd;
    if (rd > (gint64) max_rd) {
      max_rd = rd;
    }
    read_one_req(reader, req);
    ts++;
  }


  reader->sdata->reuse_dist = reuse_dist_array;
  reader->sdata->max_reuse_dist = max_rd;
  reader->sdata->reuse_dist_type = REUSE_DIST;


  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return reuse_dist_array;
}


gint64 *get_reuse_dist(reader_t *reader) {
  return _get_reuse_dist_seq(reader);
}

/* TODO: need to rewrite this function for the same reason as get_next_access_dist */
gint64 *get_future_reuse_dist(reader_t *reader) {
  /*  this function finds har far in the future, a given obj_id will be requested again,
   *  if it won't be requested again, then -1.
   *  ATTENTION: the reuse distance of the last element is at last,
   *  meaning the sequence is NOT reversed.

   *  It is the user's responsibility to release the memory of hit count array
   *  returned by this function.
   */

//  WARNING("%s has some overhead, need a rewrite\n", __func__);

  // check whether the reuse LAST_DIST computation has been finished or not
  if (reader->sdata->reuse_dist &&
      reader->sdata->reuse_dist_type == FUTURE_RD) {
    return reader->sdata->reuse_dist;
  }

  guint64 ts = 0;
  gint64 max_rd = 0;
  gint64 reuse_dist;
  get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);

  gint64 *reuse_dist_array = g_new(gint64, reader->base->n_total_req);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                           (GDestroyNotify) g_free, \
                                           NULL);
  } else {
    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }

  // create splay tree
  sTree *splay_tree = NULL;

  reader_set_read_pos(reader, 1.0);
  go_back_one_line(reader);
  read_one_req(reader, req);
  set_no_eof(reader);
  while (req->valid) {
    if (ts == reader->base->n_total_req)
      break;

    splay_tree = get_reuse_dist_add_req(req, splay_tree, hash_table, ts, &reuse_dist);
    if (reader->base->n_total_req - 1 - (long) ts < 0) {
      ERROR("array index %ld out of range\n", (long) (reader->base->n_total_req - 1 - ts));
      exit(1);
    }
    reuse_dist_array[reader->base->n_total_req - 1 - ts] = reuse_dist;
    if (reuse_dist > (gint64) max_rd)
      max_rd = reuse_dist;
    if (ts >= reader->base->n_total_req)
      break;
    read_one_req_above(reader, req);
    ts++;
  }

  // save to reader
  if (reader->sdata->reuse_dist != NULL) {
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;
  }
  reader->sdata->reuse_dist = reuse_dist_array;
  reader->sdata->max_reuse_dist = max_rd;
  reader->sdata->reuse_dist_type = FUTURE_RD;

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return reuse_dist_array;
}


/*-----------------------------------------------------------------------------
 *
 * get_reuse_time --
 *      compute reuse time (wall clock time) since last request,
 *      then save reader->sdata->REUSE_DIST
 *
 *      Notice that this is not reuse distance,
 *      even though it is stored in reuse distance array
 *      And also the time is converted to integer, so floating point timestmap
 *      loses its accuracy
 *
 * Input:
 *      reader:         the reader for data
 *
 * Return:
 *      a pointer to the reuse time array
 *
 *-----------------------------------------------------------------------------
 */
gint64 *get_reuse_time(reader_t *reader) {
  /*
   * TODO: might be better to split return result, in case the hit rate array is too large
   * Is there a better way to do this? this will cause huge amount memory
   * It is the user's responsibility to release the memory of hit count array returned by this function
   */

  // check whether REUSE_TIME computation has been done before
  if (reader->sdata->reuse_dist &&
      reader->sdata->reuse_dist_type == REUSE_TIME) {
    return reader->sdata->reuse_dist;
  }

  guint64 ts = 0;
  gint64 max_rt = 0, rt = 0;
  gpointer value;
  get_num_of_req(reader);

  gint64 *dist_array = g_new(gint64, reader->base->n_total_req);
  request_t *req = new_request(reader->base->obj_id_type);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);
  } else {
    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }

  read_one_req(reader, req);
  if (req->real_time == -1) {
    ERROR("given reader does not have real time column\n");
    abort();
  }

  while (req->valid) {
    value = g_hash_table_lookup(hash_table, req->obj_id_ptr);
    if (value != NULL) {
      rt = (gint64) req->real_time - GPOINTER_TO_SIZE (value);
    } else
      rt = -1;

    dist_array[ts] = rt;
    if (rt > (gint64) max_rt) {
      max_rt = rt;
    }

    // insert into hashtable
    if (req->obj_id_type == OBJ_ID_STR)
      g_hash_table_insert(hash_table, g_strdup((gchar *) (req->obj_id_ptr)),
                          GSIZE_TO_POINTER((gsize) (req->real_time)));

    else if (req->obj_id_type == OBJ_ID_NUM) {
      g_hash_table_insert(hash_table, GSIZE_TO_POINTER((gsize) req->obj_id_ptr),
                          GSIZE_TO_POINTER((gsize) (req->real_time)));
    }

    read_one_req(reader, req);
    ts++;
  }

  if (reader->sdata->reuse_dist != NULL) {
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;
  }
  reader->sdata->reuse_dist = dist_array;
  reader->sdata->max_reuse_dist = max_rt;
  reader->sdata->reuse_dist_type = REUSE_TIME;

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}


gint64 *cal_save_reuse_dist(reader_t *const reader,
                            const char *const save_file_loc,
                            const dist_t dist_type) {

  gint64 *rd = NULL;

  if (dist_type == REUSE_DIST)
    rd = _get_reuse_dist_seq(reader);
  else if (dist_type == FUTURE_RD)
    rd = get_future_reuse_dist(reader);
  else {
    ERROR("cannot recognize dist_type %d\n", dist_type);
    abort();
  }

  // in multi-threading, this might be a problem
  FILE *file = fopen(save_file_loc, "wb");
  fwrite(rd, sizeof(gint64), reader->base->n_total_req, file);
  fclose(file);
  return rd;
}


gint64 *load_reuse_dist(reader_t *const reader,
                        const char *const load_file_loc,
                        const dist_t dist_type) {

  get_num_of_req(reader);

  gint64 *reuse_dist_array = g_new(gint64, reader->base->n_total_req);
  FILE *file = fopen(load_file_loc, "rb");
  fread(reuse_dist_array, sizeof(gint64), reader->base->n_total_req, file);
  fclose(file);

  gint64 i, max_rd = -1;
  for (i = 0; i < reader->base->n_total_req; i++)
    if (reuse_dist_array[i] > max_rd)
      max_rd = reuse_dist_array[i];

  // save to reader
  if (reader->sdata->reuse_dist != NULL) {
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;
  }
  reader->sdata->reuse_dist = reuse_dist_array;
  reader->sdata->max_reuse_dist = max_rd;
  reader->sdata->reuse_dist_type = dist_type;
  return reuse_dist_array;
}


gint64 *_get_last_access_dist_seq(reader_t *reader, void (*funcPtr)(reader_t *, request_t *)) {
  // check whether computation has been done before
  dist_t dist_type = funcPtr == read_one_req ? LAST_DIST : NEXT_DIST;
  if (reader->sdata->reuse_dist) {
    if (funcPtr == read_one_req && reader->sdata->reuse_dist_type == dist_type)
      return reader->sdata->reuse_dist;
  }

  guint64 n_req = get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);
  gint64 *dist_array = g_new(gint64, n_req);


  // create hashtable
  GHashTable *hash_table;
  if (req->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (req->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                               (GDestroyNotify) g_free, \
                                               NULL);
  } else {
    ERROR("unknown obj_id_type: %c\n", req->obj_id_type);
    abort();
  }

  guint64 ts = 0;
  gint64 dist, max_dist = 0;
  gboolean is_csv_with_header = FALSE;
  if (reader->base->trace_type == CSV_TRACE) {
    csv_params_t *params = reader->reader_params;
    if (params->has_header)
      is_csv_with_header = TRUE;
  }

  if (funcPtr == read_one_req) {
    read_one_req(reader, req);
  } else if (funcPtr == read_one_req_above) {
    reader_set_read_pos(reader, 1.0);
    if (go_back_one_line(reader) != 0) ERROR("error when going back one line\n");
    read_one_req(reader, req);
    set_no_eof(reader);
  } else {
    ERROR("unknown function pointer received in %s\n", __func__);
    abort();
  }

  while (req->valid) {
    dist = _get_last_dist_add_req(req, hash_table, ts);
    if (dist > max_dist)
      max_dist = dist;
    if (funcPtr == read_one_req) {
      dist_array[ts] = dist;
    } else if (funcPtr == read_one_req_above) {
      if ((gint64) (n_req - 1 - ts) < 0) {
        if ((gint64) n_req - 1 - ts == -1 && is_csv_with_header){
          funcPtr(reader, req);
          ts++;
          continue;
        }
        else {
          ERROR("index error in _get_last_access_dist_seq when get next access dist\n");
          abort();
        }
      }
      dist_array[n_req - 1 - ts] = dist;
    }
    funcPtr(reader, req);
    ts++;
  }


  if (reader->sdata->reuse_dist != NULL) {
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;
  }
  reader->sdata->reuse_dist = dist_array;
  reader->sdata->max_reuse_dist = max_dist;
  reader->sdata->reuse_dist_type = dist_type;

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}


gint64 *get_last_access_dist(reader_t *reader) {
  return _get_last_access_dist_seq(reader, read_one_req);
}


// TODO: rewrite this
/**
 * get the dist to next access
 * note: it is better to rewrite this function to not use read_one_req_above, which has high overhead
 * @param reader
 * @return
 */
gint64 *get_next_access_dist(reader_t *reader) {
//  WARNING("%s has some overhead, need a rewrite\n", __func__);
  return _get_last_access_dist_seq(reader, read_one_req_above);
}



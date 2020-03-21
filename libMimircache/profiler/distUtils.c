//
// Created by Juncheng Yang on 11/24/19.
//

#include "../include/mimircache/distUtils.h"
#include "../include/mimircache/profilerUtils.h"
#include "distUtilsInternal.h"
#include "utilsInternal.h"


#include <sys/stat.h>
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************
* sequential version of get_reuse_dist
* @param reader
* @return
*/
gint64 *_get_reuse_dist_seq(reader_t *reader) {
  guint64 ts = 0;
  gint64 max_rd = 0;
  gint64 rd = 0;
  get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);

  gint64 *reuse_dist_array = g_new(gint64, reader->base->n_total_req);

  // create hashtable
  GHashTable *hash_table = create_hash_table(reader, NULL, NULL,
      (GDestroyNotify)g_free, NULL);
//  if (reader->base->obj_id_type == OBJ_ID_NUM) {
//    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
//  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
//    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);
//  } else {
//    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
//    abort();
//  }

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

  gint64 ts = 0;
  gint64 max_rd = 0;
  gint64 reuse_dist;
  get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);

  gint64 *reuse_dist_array = g_new(gint64, reader->base->n_total_req);

  // create hashtable
  GHashTable *hash_table = create_hash_table(reader, NULL, NULL,
      (GDestroyNotify) g_free, NULL);

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

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return reuse_dist_array;
}


gint64 *_get_last_access_dist_seq(reader_t *reader, void (*funcPtr)(reader_t *, request_t *)) {
  guint64 n_req = get_num_of_req(reader);
  request_t *req = new_request(reader->base->obj_id_type);
  gint64 *dist_array = g_new(gint64, n_req);


  // create hashtable
  GHashTable *hash_table = create_hash_table(reader, NULL, NULL,
                                             (GDestroyNotify) g_free, NULL);

  gint64 ts = 0;
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
        if ((gint64) n_req - 1 - ts == -1 && is_csv_with_header) {
          funcPtr(reader, req);
          ts++;
          continue;
        } else {
          ERROR("index error in _get_last_access_dist_seq when get next access dist\n");
          abort();
        }
      }
      dist_array[n_req - 1 - ts] = dist;
    }
    funcPtr(reader, req);
    ts++;
  }


//  if (reader->sdata->reuse_dist != NULL) {
//    g_free(reader->sdata->reuse_dist);
//    reader->sdata->reuse_dist = NULL;
//  }
//  reader->sdata->reuse_dist = dist_array;
//  reader->sdata->max_reuse_dist = max_dist;
//  reader->sdata->reuse_dist_type = dist_type;

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


gint64 *get_reuse_time(reader_t *reader) {
  guint64 ts = 0;
  gint64 max_rt = 0, rt = 0;
  gpointer value;
  get_num_of_req(reader);

  gint64 *dist_array = g_new(gint64, reader->base->n_total_req);
  request_t *req = new_request(reader->base->obj_id_type);

  // create hashtable
  GHashTable *hash_table = create_hash_table(reader, NULL, NULL,
                                             (GDestroyNotify)g_free, NULL);

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

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}


void save_dist(reader_t *const reader, gint64 *dist_array, const char *const path, dist_t dist_type) {

  // in multi-threading, this might be a problem
  char *file_path = (char*)malloc(strlen(path)+8);
  sprintf(file_path, "%s.%d", path, dist_type);
  FILE *file = fopen(file_path, "wb");
  fwrite(dist_array, sizeof(gint64), reader->base->n_total_req, file);
  fclose(file);
  free(file_path);
}


gint64 *load_dist(reader_t *const reader, const char *const path, dist_t dist_type) {

  char *file_path = (char*)malloc(strlen(path)+8);
  sprintf(file_path, "%s.%d", path, dist_type);
  FILE *file = fopen(path, "rb");

  int fd = fileno(file); //if you have a stream (e.g. from fopen), not a file descriptor.
  struct stat buf;
  fstat(fd, &buf);
  off_t size = buf.st_size;

  get_num_of_req(reader);
  assert(size == sizeof(gint64)*reader->base->n_total_req);

  gint64 *dist_array = g_new(gint64, reader->base->n_total_req);
  fread(dist_array, sizeof(gint64), reader->base->n_total_req, file);
  fclose(file);

  return dist_array;
}




#ifdef __cplusplus
}
#endif

//
// Created by Juncheng Yang on 11/24/19.
//

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <mymath.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../dataStructure/include/splay.h"
#include "../utils/include/utilsInternal.h"
#include "include/test_dist.h"
#include "libCacheSim/dist.h"

/* https://stackoverflow.com/questions/46213840/get-rid-of-warning-implicit-declaration-of-function-fileno-in-flex
 */
int fileno(FILE *stream);

/***********************************************************
 * sequential version of get_stack_dist
 * @param reader
 * @return
 */
gint64 *_get_stack_dist_seq(reader_t *reader) {
  guint64 ts = 0;
  gint64 max_rd = 0;
  gint64 rd = 0;
  get_num_of_req(reader);
  request_t *req = new_request();

  gint64 *stack_dist_array = g_new(gint64, get_num_of_req(reader));

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  // create splay tree
  sTree *splay_tree = NULL;

  read_one_req(reader, req);
  while (req->valid) {
    splay_tree = get_stack_dist_add_req(req, splay_tree, hash_table, ts, &rd);
    stack_dist_array[ts] = rd;
    if (rd > (gint64)max_rd) {
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
  return stack_dist_array;
}

gint64 *get_stack_dist(reader_t *reader) { return _get_stack_dist_seq(reader); }

/* TODO: need to rewrite this function for the same reason as
 * get_next_access_dist */
gint64 *get_future_stack_dist(reader_t *reader) {

  gint64 ts = 0;
  gint64 max_rd = 0;
  gint64 stack_dist;
  get_num_of_req(reader);
  request_t *req = new_request();

  gint64 *stack_dist_array = g_new(gint64, get_num_of_req(reader));

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  // create splay tree
  sTree *splay_tree = NULL;

  reader_set_read_pos(reader, 1.0);
  go_back_one_line(reader);
  read_one_req(reader, req);
  set_no_eof(reader);
  while (req->valid) {
    if (ts == get_num_of_req(reader))
      break;

    splay_tree =
        get_stack_dist_add_req(req, splay_tree, hash_table, ts, &stack_dist);
    if (get_num_of_req(reader) - 1 - (long)ts < 0) {
      ERROR("array index %ld out of range\n",
            (long)(get_num_of_req(reader) - 1 - ts));
      exit(1);
    }
    stack_dist_array[get_num_of_req(reader) - 1 - ts] = stack_dist;
    if (stack_dist > (gint64)max_rd)
      max_rd = stack_dist;
    if (ts >= get_num_of_req(reader))
      break;
    read_one_req_above(reader, req);
    ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return stack_dist_array;
}

gint64 *_get_last_access_dist(reader_t *reader,
                              int (*funcPtr)(reader_t *, request_t *)) {
  guint64 n_req = get_num_of_req(reader);
  request_t *req = new_request();
  gint64 *dist_array = g_new(gint64, n_req);

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  gint64 ts = 0;
  gint64 dist, max_dist = 0;

  if (funcPtr == read_one_req) {
    read_one_req(reader, req);
  } else if (funcPtr == read_one_req_above) {
    reader_set_read_pos(reader, 1.0);
    if (go_back_one_line(reader) != 0)
      ERROR("error when going back one line\n");
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
      if ((gint64)(n_req - 1 - ts) < 0) {
        if (get_trace_type(reader) == CSV_TRACE) {
          funcPtr(reader, req);
          ts++;
          continue;
        } else {
          ERROR("index error in _get_last_access_dist when get next access "
                "dist\n");
          abort();
        }
      }
      dist_array[n_req - 1 - ts] = dist;
    }
    funcPtr(reader, req);
    ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}

gint64 *get_last_access_dist(reader_t *reader) {
  return _get_last_access_dist(reader, read_one_req);
}

gint64 *get_first_access_dist(reader_t *reader) {
  guint64 n_req = get_num_of_req(reader);
  request_t *req = new_request();
  gint64 *dist_array = g_new(gint64, n_req);

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  gint64 ts = 0;
  gint64 dist, max_dist = 0;
  read_one_req(reader, req);

  while (req->valid) {
    dist = _get_first_dist_add_req(req, hash_table, ts);
    if (dist > max_dist)
      max_dist = dist;
    dist_array[ts] = dist;
    read_one_req(reader, req);
    ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}

// TODO: rewrite this
/**
 * get the dist to next access
 * note: it is better to rewrite this function to not use read_one_req_above,
 * which has high overhead
 * @param reader
 * @return
 */
gint64 *get_next_access_dist(reader_t *reader) {
  //  WARNING("%s has some overhead, need a rewrite\n", __func__);
  return _get_last_access_dist(reader, read_one_req_above);
}

gint64 *get_reuse_time(reader_t *reader) {
  guint64 ts = 0;
  gint64 max_rt = 0, rt = 0;
  guint64 value;
  get_num_of_req(reader);

  gint64 *dist_array = g_new(gint64, get_num_of_req(reader));
  request_t *req = new_request();

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  read_one_req(reader, req);
  if (req->real_time == -1) {
    ERROR("given reader does not have real time column\n");
    abort();
  }

  while (req->valid) {
    value = GPOINTER_TO_SIZE(
        g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int)));
    if (value != 0) {
      rt = (gint64)req->real_time - GPOINTER_TO_SIZE(value);
    } else
      rt = -1;

    dist_array[ts] = rt;
    if (rt > (gint64)max_rt) {
      max_rt = rt;
    }

    // insert into hashtable
    g_hash_table_insert(hash_table, GSIZE_TO_POINTER((gsize)req->obj_id_int),
                        GSIZE_TO_POINTER((gsize)(req->real_time)));

    read_one_req(reader, req);
    ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_array;
}

void save_dist(reader_t *const reader, gint64 *dist_array,
               const char *const path, dist_t dist_type) {

  // in multi-threading, this might be a problem
  char *file_path = (char *)malloc(strlen(path) + 8);
  sprintf(file_path, "%s.%d", path, dist_type);
  FILE *file = fopen(file_path, "wb");
  fwrite(dist_array, sizeof(gint64), get_num_of_req(reader), file);
  fclose(file);
  free(file_path);
}

gint64 *load_dist(reader_t *const reader, const char *const path,
                  dist_t dist_type) {

  char file_path[1024];
  sprintf(file_path, "%s.%d", path, dist_type);
  FILE *file = fopen(file_path, "rb");
  if (file == NULL) {
    perror(file_path);
    abort();
  }

  int fd = fileno(
      file); // if you have a stream (e.g. from fopen), not a file descriptor.
  struct stat buf;
  fstat(fd, &buf);

  get_num_of_req(reader);
  assert(buf.st_size == sizeof(gint64) * get_num_of_req(reader));

  gint64 *dist_array = g_new(gint64, get_num_of_req(reader));
  fread(dist_array, sizeof(gint64), get_num_of_req(reader), file);
  fclose(file);

  return dist_array;
}

gint32 *_cnt_dist(gint64 *dist, gint64 n_req, double log_base,
                  gint64 *n_dist_cnt) {
  gint64 max_dist, max_dist_idx;
  find_max(dist, n_req, &max_dist, &max_dist_idx);
  double log_base_div = log(log_base);
  *n_dist_cnt = (gint64)(log(max_dist) / log_base_div) + 1;
  gint32 *dist_cnt = g_new0(gint32, *n_dist_cnt);
  for (gint64 i = 0; i < n_req; i++) {
    if (dist[i] != -1) {
      dist_cnt[(gint)(log(dist[i]) / log_base_div)] += 1;
    }
  }
  //  printf("%ld %ld\n", dist_cnt[20], dist_cnt[*n_dist_cnt-1]);
  return dist_cnt;
}

gint32 *get_reuse_time_cnt_in_bins0(reader_t *reader, double log_base,
                                    gint64 *n_dist_cnt) {
  gint64 *dist = get_reuse_time(reader);
  gint32 *dist_cnt =
      _cnt_dist(dist, get_num_of_req(reader), log_base, n_dist_cnt);
  g_free(dist);
  return dist_cnt;
}

gint32 *get_last_access_dist_cnt_in_bins0(reader_t *reader, double log_base,
                                          gint64 *n_dist_cnt) {
  gint64 *dist = get_last_access_dist(reader);
  gint32 *dist_cnt =
      _cnt_dist(dist, get_num_of_req(reader), log_base, n_dist_cnt);
  g_free(dist);
  return dist_cnt;
}

gint32 *get_first_access_dist_cnt_in_bins0(reader_t *reader, double log_base,
                                           gint64 *n_dist_cnt) {
  gint64 *dist = get_first_access_dist(reader);
  gint32 *dist_cnt =
      _cnt_dist(dist, get_num_of_req(reader), log_base, n_dist_cnt);
  g_free(dist);
  return dist_cnt;
}

gint32 *get_reuse_time_cnt_in_bins(reader_t *reader, double log_base,
                                   gint64 *n_dist_cnt) {

  gint64 rt = 0;
  gint64 value;
  gint32 pos = 0;

  // make sure large enough
  gint32 dist_cnt_array_size = (gint32)(log(1e16) / log(log_base));
  gint32 *dist_cnt_array = g_new0(gint32, dist_cnt_array_size);

  request_t *req = new_request();

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  read_one_req(reader, req);

  while (req->valid) {
    value = GPOINTER_TO_SIZE(
        g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int)));
    if (value != 0) {
      rt = (gint64)req->real_time - GPOINTER_TO_SIZE(value);
      pos = (gint32)(log((double)rt + 1) / log(log_base));
      dist_cnt_array[pos]++;

      if (pos > *n_dist_cnt) {
        *n_dist_cnt = pos;
      }
    }

    // insert into hashtable
    g_hash_table_insert(hash_table, GSIZE_TO_POINTER((gsize)req->obj_id_int),
                        GSIZE_TO_POINTER((gsize)(req->real_time)));

    read_one_req(reader, req);
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_cnt_array;
}

gint32 *get_last_access_dist_cnt_in_bins(reader_t *reader, double log_base,
                                         gint64 *n_dist_cnt) {

  gint64 cur_ts = 0, last_access_dist = 0;
  gint64 value;
  gint32 pos = 0;

  // make sure large enough
  gint32 dist_cnt_array_size = (gint32)(log(1e16) / log(log_base));
  gint32 *dist_cnt_array = g_new0(gint32, dist_cnt_array_size);

  request_t *req = new_request();

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  read_one_req(reader, req);

  while (req->valid) {
    value = GPOINTER_TO_SIZE(
        g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int)));
    if (value != 0) {
      last_access_dist = (gint64)cur_ts - GPOINTER_TO_SIZE(value);
      pos = (gint32)(log((double)last_access_dist + 1) / log(log_base));
      dist_cnt_array[pos]++;

      if (pos > *n_dist_cnt) {
        *n_dist_cnt = pos;
      }
    }

    // insert into hashtable
    g_hash_table_insert(hash_table, GSIZE_TO_POINTER((gsize)req->obj_id_int),
                        GSIZE_TO_POINTER((gsize)(req->real_time)));

    read_one_req(reader, req);
    cur_ts += 1;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_cnt_array;
}

gint32 *get_first_access_dist_cnt_in_bins(reader_t *reader, double log_base,
                                          gint64 *n_dist_cnt) {

  gint64 cur_ts = 0, first_access_dist;
  guint64 value;
  gint32 pos = 0;

  // make sure large enough
  gint32 dist_cnt_array_size = (gint32)(log(1e16) / log(log_base));
  gint32 *dist_cnt_array = g_new0(gint32, dist_cnt_array_size);

  request_t *req = new_request();

  // create hashtable
  GHashTable *hash_table =
      create_hash_table(reader, NULL, NULL, (GDestroyNotify)g_free, NULL);

  read_one_req(reader, req);

  while (req->valid) {
    value = GPOINTER_TO_SIZE(
        g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id_int)));
    if (value != 0) {
      first_access_dist = (gint64)cur_ts - GPOINTER_TO_SIZE(value);
      pos = (gint32)(log((double)first_access_dist + 1) / log(log_base));
      dist_cnt_array[pos]++;
      if (pos > *n_dist_cnt) {
        *n_dist_cnt = pos;
      }
    } else {
      // insert into hashtable
      g_hash_table_insert(hash_table, GSIZE_TO_POINTER((gsize)req->obj_id_int),
                          GSIZE_TO_POINTER((gsize)(req->real_time)));
    }

    cur_ts += 1;
    read_one_req(reader, req);
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);
  return dist_cnt_array;
}

#ifdef __cplusplus
}
#endif

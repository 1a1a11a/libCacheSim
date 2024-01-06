//
// Created by Juncheng Yang on 11/24/19.
//

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../dataStructure/splay.h"
#include "../include/libCacheSim/dist.h"
#include "../include/libCacheSim/macro.h"

/***********************************************************
 * this function is called by _get_dist,
 * it return distance/age (reference count) to its first/last request
 * note that the distance between req at t and at t+1 is 1,
 * the calculated dist = cur_ts - last_ts
 *
 *
 *
 *
 * @param req           request_t contains current request
 * @param hash_table    the hashtable storing last/first access timestamp
 * @param curr_ts       current timestamp
 * @param dist_type     DIST_SINCE_LAST_ACCESS or DIST_SINCE_FIRST_ACCESS
 * @return              distance to last access
 */
int64_t get_access_dist_add_req(const request_t *req, GHashTable *hash_table,
                                const int64_t curr_ts,
                                const dist_type_e dist_type) {
  gpointer gp = g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id));
  int64_t ret = -1;
  if (gp == NULL) {
    // it has not been requested before
    ret = -1;
  } else {
    // it has been requested before
    int64_t old_ts = (int64_t)GPOINTER_TO_SIZE(gp);
    ret = curr_ts - old_ts;
  }

  if (dist_type == DIST_SINCE_LAST_ACCESS) {
    /* update last access time */
    g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id),
                        GSIZE_TO_POINTER((gsize)curr_ts));
  } else if (dist_type == DIST_SINCE_FIRST_ACCESS) {
    /* do nothing */
  } else {
    ERROR("dist_type %d not supported in access_dist\n", dist_type);
  }
  return ret;
}

/***********************************************************
 * this function is used for computing stack distance for each request
 * it maintains a hashmap and a splay tree,
 * time complexity is O(log(N)), N is the number of unique elements
 *
 *
 * @param req           request_t contains current request
 * @param splay_tree        a double pointer to the splay tree struct (will be
 * updated in this function)
 * @param hash_table        hashtable for remember last request timestamp
 * @param curr_ts           current timestamp
 * @return                  stack distance
 */
int64_t get_stack_dist_add_req(const request_t *req, sTree **splay_tree,
                               GHashTable *hash_table, const int64_t curr_ts,
                               int64_t *last_access_ts) {
  gpointer gp = g_hash_table_lookup(hash_table, GSIZE_TO_POINTER(req->obj_id));

  int64_t ret = -1;
  sTree *newtree;
  if (gp == NULL) {
    // first time access
    if (last_access_ts != NULL) {
      *last_access_ts = -1;
    }
    ret = -1;
    newtree = insert(curr_ts, *splay_tree);
  } else {
    // not first time access
    int64_t old_ts = (int64_t)GPOINTER_TO_SIZE(gp);
    if (last_access_ts != NULL) {
      *last_access_ts = old_ts;
    }
    newtree = splay(old_ts, *splay_tree);
    ret = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(curr_ts, newtree);
  }

  g_hash_table_insert(hash_table, GSIZE_TO_POINTER(req->obj_id),
                      (gpointer)GSIZE_TO_POINTER((gsize)curr_ts));

  *splay_tree = newtree;

  return ret;
}

/***********************************************************
 * sequential version of get_stack_dist
 * @param reader
 * @return
 */
int32_t *get_stack_dist(reader_t *reader, const dist_type_e dist_type,
                        int64_t *array_size) {
  int64_t curr_ts = 0;
  int64_t last_access_ts = 0;
  int64_t stack_dist = 0;
  request_t *req = new_request();
  *array_size = get_num_of_req(reader);

  int32_t *stack_dist_array = malloc(sizeof(int32_t) * get_num_of_req(reader));
  if (dist_type == FUTURE_STACK_DIST) {
    for (int64_t i = 0; i < get_num_of_req(reader); i++) {
      stack_dist_array[i] = -1;
    }
  }

  GHashTable *hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  // create splay tree
  sTree *splay_tree = NULL;

  read_one_req(reader, req);
  while (req->valid) {
    stack_dist = get_stack_dist_add_req(req, &splay_tree, hash_table, curr_ts,
                                        &last_access_ts);
    if (stack_dist > (int64_t)UINT32_MAX) {
      ERROR("stack distance %ld is larger than UINT32_MAX\n", (long)stack_dist);
      abort();
    }
    if (dist_type == STACK_DIST) {
      stack_dist_array[curr_ts] = stack_dist;
    } else if (dist_type == FUTURE_STACK_DIST) {
      if (last_access_ts != -1) {
        stack_dist_array[last_access_ts] = stack_dist;
      }
    } else {
      ERROR("dist_type %d is not supported in stack distance calculation\n",
            dist_type);
    }
    read_one_req(reader, req);
    curr_ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return stack_dist_array;
}

int32_t *get_access_dist(reader_t *reader, const dist_type_e dist_type,
                         int64_t *array_size) {
  int64_t curr_ts = 0;
  int64_t dist = 0;
  request_t *req = new_request();
  *array_size = get_num_of_req(reader);
  int32_t *dist_array = malloc(sizeof(int32_t) * get_num_of_req(reader));

  GHashTable *hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  read_one_req(reader, req);

  while (req->valid) {
    dist = get_access_dist_add_req(req, hash_table, curr_ts, dist_type);
    if (dist > (int64_t)UINT32_MAX) {
      ERROR("access distance %ld is larger than UINT32_MAX\n", (long)dist);
      abort();
    }

    dist_array[curr_ts] = dist;
    read_one_req(reader, req);
    curr_ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  reset_reader(reader);

  return dist_array;
}

void save_dist(reader_t *const reader, const int32_t *dist_array,
               int64_t array_size, const char *const ofilepath,
               const dist_type_e dist_type) {
  char *file_path = (char *)malloc(strlen(ofilepath) + 128);
  sprintf(file_path, "%s.%s", ofilepath, g_dist_type_name[dist_type]);
  FILE *file = fopen(file_path, "wb");
  fwrite(dist_array, sizeof(int32_t), get_num_of_req(reader), file);
  fclose(file);
  free(file_path);
}

void save_dist_txt(reader_t *const reader, const int32_t *dist_array,
                   int64_t array_size, const char *const ofilepath,
                   const dist_type_e dist_type) {
  char *file_path = (char *)malloc(strlen(ofilepath) + 128);
  sprintf(file_path, "%s.%s.txt", ofilepath, g_dist_type_name[dist_type]);
  FILE *file = fopen(file_path, "w");
  for (int i = 0; i < array_size; i++) {
    fprintf(file, "%d\n", dist_array[i]);
  }
  fclose(file);
  free(file_path);
}

int32_t *load_dist(reader_t *const reader, const char *const ifilepath,
                   int64_t *array_size) {
  FILE *file = fopen(ifilepath, "rb");
  if (file == NULL) {
    perror(ifilepath);
    abort();
  }

  *array_size = get_num_of_req(reader);

  int fd = fileno(file);
  struct stat buf;
  fstat(fd, &buf);
  assert(buf.st_size == sizeof(int32_t) * get_num_of_req(reader));

  int32_t *dist_array = malloc(sizeof(int32_t) * get_num_of_req(reader));
  size_t n_read =
      fread(dist_array, sizeof(int32_t), get_num_of_req(reader), file);
  assert(n_read == get_num_of_req(reader));

  fclose(file);

  return dist_array;
}

void cnt_dist(const int32_t *dist_array, const int64_t array_size,
              GHashTable *hash_table) {
  for (uint64_t i = 0; i < array_size; i++) {
    int64_t dist = dist_array[i] == -1 ? INT64_MAX : dist_array[i];
    gpointer gp_dist = GSIZE_TO_POINTER((gsize)dist);
    int64_t old_cnt = (int64_t)g_hash_table_lookup(hash_table, gp_dist);
    g_hash_table_replace(hash_table, gp_dist, GSIZE_TO_POINTER(old_cnt + 1));
  }
}

void _write_dist_cnt(gpointer key, gpointer value, gpointer user_data) {
  int64_t dist = (int64_t)GPOINTER_TO_SIZE(key);
  int64_t cnt = (int64_t)GPOINTER_TO_SIZE(value);
  FILE *file = (FILE *)user_data;
  fprintf(file, "%ld:%ld, ", (long)dist, (long)cnt);
}

void save_dist_as_cnt_txt(reader_t *const reader, const int32_t *dist_array,
                          const int64_t array_size, const char *const ofilepath,
                          const dist_type_e dist_type) {
  assert(get_num_of_req(reader) == array_size);

  char *file_path = (char *)malloc(strlen(ofilepath) + 128);
  sprintf(file_path, "%s.%s.cnt", ofilepath, g_dist_type_name[dist_type]);
  FILE *file = fopen(file_path, "w");

  GHashTable *hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  cnt_dist(dist_array, get_num_of_req(reader), hash_table);

  g_hash_table_foreach(hash_table, (GHFunc)_write_dist_cnt, file);

  g_hash_table_destroy(hash_table);

  fclose(file);
  free(file_path);
}

#ifdef __cplusplus
}
#endif

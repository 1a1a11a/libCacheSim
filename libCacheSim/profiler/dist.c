//
// Created by Juncheng Yang on 11/24/19.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/dist.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../dataStructure/splay.h"
#include "../include/conversion.h"
#include "../include/libCacheSim/hashmap.h"
#include "../include/libCacheSim/hashmap_defs.h"
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
int64_t get_access_dist_add_req(const request_t *req, hashmap_t *hash_table,
                                const int64_t curr_ts,
                                const dist_type_e dist_type) {
  void *gp =
      hashmap_get(hash_table, int_to_cptr(req->obj_id), sizeof(obj_id_t));
  int64_t ret = -1;
  if (gp == NULL) {
    // it has not been requested before
    ret = -1;
  } else {
    // it has been requested before
    int64_t old_ts = ptr_to_long(gp);
    ret = curr_ts - old_ts;
  }

  if (dist_type == DIST_SINCE_LAST_ACCESS) {
    /* update last access time */
    hashmap_put(hash_table, int_to_cptr(req->obj_id), sizeof(obj_id_t),
                int_to_ptr(curr_ts));
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
                               hashmap_t *hash_table, const int64_t curr_ts,
                               int64_t *last_access_ts) {
  void *gp =
      hashmap_get(hash_table, int_to_cptr(req->obj_id), sizeof(obj_id_t));

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
    int64_t old_ts = (int64_t)gp;
    if (last_access_ts != NULL) {
      *last_access_ts = old_ts;
    }
    newtree = splay(old_ts, *splay_tree);
    ret = node_value(newtree->right);
    newtree = splay_delete(old_ts, newtree);
    newtree = insert(curr_ts, newtree);
  }

  hashmap_put(hash_table, int_to_cptr(req->obj_id), sizeof(obj_id_t),
              int_to_ptr(curr_ts));

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

  hashmap_t *hash_table = malloc(sizeof(hashmap_t));
  hashmap_create_options_t options = {.initial_capacity = 16,
                                      .comparer = obj_id_comparer,
                                      .hasher = obj_id_hasher};
  hashmap_create_ex(options, hash_table);

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
  hashmap_destroy(hash_table);
  free(hash_table);
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

  hashmap_t *hash_table = malloc(sizeof(hashmap_t));
  hashmap_create_options_t options = {.initial_capacity = 16,
                                      .comparer = obj_id_comparer,
                                      .hasher = obj_id_hasher};
  hashmap_create_ex(options, hash_table);

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
  hashmap_destroy(hash_table);
  free(hash_table);
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
  assert((size_t)buf.st_size == sizeof(int32_t) * get_num_of_req(reader));

  int32_t *dist_array = malloc(sizeof(int32_t) * get_num_of_req(reader));
  int64_t n_read =
      fread(dist_array, sizeof(int32_t), get_num_of_req(reader), file);
  assert(n_read == get_num_of_req(reader));

  fclose(file);

  return dist_array;
}

void cnt_dist(const int32_t *dist_array, const int64_t array_size,
              hashmap_t *hash_table) {
  for (int64_t i = 0; i < array_size; i++) {
    int64_t dist = dist_array[i] == -1 ? INT64_MAX : dist_array[i];
    int64_t old_cnt =
        (int64_t)hashmap_get(hash_table, int_to_cptr(dist), sizeof(int64_t));
    hashmap_put(hash_table, int_to_cptr(dist), sizeof(int64_t),
                int_to_ptr(old_cnt + 1));
  }
}

/**
 * helper function for save_dist_as_cnt_txt
 **/
static int _write_dist_cnt(void *const user_data,
                           struct hashmap_element_s *const e) {
  FILE *file = (FILE *)user_data;
  int64_t dist = (long)(e->key);
  int64_t cnt = (long)(e->data);
  fprintf(file, "%ld:%ld, ", dist, cnt);
  return 1;
}

void save_dist_as_cnt_txt(reader_t *const reader, const int32_t *dist_array,
                          const int64_t array_size, const char *const ofilepath,
                          const dist_type_e dist_type) {
  assert((int64_t)get_num_of_req(reader) == array_size);

  char *file_path = (char *)malloc(strlen(ofilepath) + 128);
  sprintf(file_path, "%s.%s.cnt", ofilepath, g_dist_type_name[dist_type]);
  FILE *file = fopen(file_path, "w");

  hashmap_t *hash_table = malloc(sizeof(hashmap_t));
  hashmap_create_options_t options = {.initial_capacity = 16,
                                      .comparer = obj_id_comparer,
                                      .hasher = obj_id_hasher};
  hashmap_create_ex(options, hash_table);

  cnt_dist(dist_array, get_num_of_req(reader), hash_table);

  hashmap_iterate_pairs(hash_table, _write_dist_cnt, file);

  hashmap_destroy(hash_table);
  free(hash_table);

  fclose(file);
  free(file_path);
}

#ifdef __cplusplus
}
#endif

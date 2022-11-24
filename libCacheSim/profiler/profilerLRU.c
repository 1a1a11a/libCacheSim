//
//  LRUAnalyzer.c
//  LRUAnalyzer
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../dataStructure/splay.h"
#include "../include/libCacheSim/profilerLRU.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t get_stack_dist_add_req(const request_t *req, sTree **splay_tree,
                               GHashTable *hash_table, const int64_t curr_ts,
                               int64_t *last_access_ts);

guint64 *_get_lru_hit_cnt(reader_t *reader, gint64 size);

double *get_lru_obj_miss_ratio_curve(reader_t *reader, gint64 size) {
  return get_lru_obj_miss_ratio(reader, size);
}

double *get_lru_obj_miss_ratio(reader_t *reader, gint64 size) {
  double n_req = (double)get_num_of_req(reader);
  double *miss_ratio_array = g_new(double, size + 1);

  guint64 *miss_count_array = _get_lru_miss_cnt(reader, size);
  assert(miss_count_array[0] == get_num_of_req(reader));

  for (gint64 i = 0; i < size + 1; i++) {
    miss_ratio_array[i] = miss_count_array[i] / n_req;
  }
  g_free(miss_count_array);
  return miss_ratio_array;
}

guint64 *_get_lru_miss_cnt(reader_t *reader, gint64 size) {
  guint64 n_req = get_num_of_req(reader);
  guint64 *miss_cnt = _get_lru_hit_cnt(reader, size);
  for (gint64 i = 0; i < size + 1; i++) {
    miss_cnt[i] = n_req - miss_cnt[i];
  }
  return miss_cnt;
}

/**
 * get hit count for size 0~size,
 * non-parallel version
 *
 * @param reader: reader for reading data
 * @param size: the max cache size, if -1, then it uses the maximum size
 */

guint64 *_get_lru_hit_cnt(reader_t *reader, gint64 size) {
  guint64 ts = 0;
  gint64 stack_dist;
  guint64 *hit_count_array = g_new0(guint64, size + 1);
  request_t *req = new_request();

  // create hash table and splay tree
  GHashTable *hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  sTree *splay_tree = NULL;

  read_one_req(reader, req);
  while (req->valid) {
    stack_dist = get_stack_dist_add_req(req, &splay_tree, hash_table, ts, NULL);

    if (stack_dist == -1)
      // cold miss
      ;
    else {
      if (stack_dist + 1 <= size)
        /* + 1 here because reuse stack_dist is 0 for consecutive accesses */
        hit_count_array[stack_dist + 1] += 1;
    }
    read_one_req(reader, req);
    ts++;
  }

  // change to accumulative, so that hit_count_array[x] is the hit count for
  // size x
  for (gint64 i = 1; i < size + 1; i++) {
    hit_count_array[i] = hit_count_array[i] + hit_count_array[i - 1];
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return hit_count_array;
}

#ifdef __cplusplus
}
#endif

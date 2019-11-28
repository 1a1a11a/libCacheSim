//
//  LRUAnalyzer.c
//  LRUAnalyzer
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../include/mimircache/profilerLRU.h"
#include "../include/mimircache/logging.h"
#include "murmur3.h"
#include "splay.h"

#ifdef __reqlusplus
extern "C"
{
#endif



guint64 *get_lru_miss_count_seq(reader_t *reader,
                                gint64 size){
  guint64* hc = get_lru_hit_count_seq(reader, size);
  guint64 req_cnt = get_num_of_req(reader);
  if (size == -1)
    size = req_cnt;
  for (gint64 i=0; i<size+1; i++){
    hc[i] = req_cnt - hc[1];
  }
  return hc;
}

double *get_lru_miss_ratio_seq(reader_t *reader,
                               gint64 size){
  double* hr = get_lru_hit_ratio_seq(reader, size);
  if (size == -1)
    size = get_num_of_req(reader);
  for (gint64 i=0; i<size+1; i++){
    hr[i] = 1 - hr[1];
  }
  return hr;
}

/**
 * get hit count for 0~given cache size,
 * non-parallel version
 *
 * @param reader: reader for reading data
 * @param size: the max profiling size, if -1, then the maximum possible size
 */

guint64 *get_lru_hit_count_seq(reader_t *reader,
                               gint64 size) {
  /* get the hit count, if size==-1, then do all the counting, otherwise,
   * treat the ones with reuse distance larger than size as out of range,
   * and put it in the second to the last bucket of hit_count_array
   * in other words, 0~size(included) are for counting REUSE_DIST=0~size-1, size+1 is
   * out of range, size+2 is cold miss, so total is size+3 buckets
   */

  /*
   size: the size of cache, if passed -1, then choose maximum size
   */

  guint64 ts = 0;
  gint64 reuse_dist;
  guint64 *hit_count_array;

  if (reader->base->n_total_req == -1)
    get_num_of_req(reader);

  if (size == -1)
    size = reader->base->n_total_req;

  /* for a cache_size=size, we need size+1 bucket for size 0~size(included),
   * the last element(size+1) is used for storing count of reuse distance > size
   * if size==reader->base->n_total_req, then the last two is not used
   */
  hit_count_array = g_new0(guint64, size + 3);


  // req struct creation and initialization
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

  // create splay tree
  sTree *splay_tree = NULL;

  read_one_req(reader, req);
  while (req->valid) {
    splay_tree = get_reuse_dist_add_req(req, splay_tree, hash_table, ts, &reuse_dist);
    if (reuse_dist == -1)
      hit_count_array[size + 2] += 1;
    else if (reuse_dist >= size)
      hit_count_array[size + 1] += 1;
    else
      /* + 1 here because reuse LAST_DIST is 0 if two consecutive accesses */
      hit_count_array[reuse_dist + 1] += 1;
    read_one_req(reader, req);
    ts++;
  }

  // clear the last two counter as this will be deprecated soon to provide similar API as run_trace
  hit_count_array[size + 1] = 0;
  hit_count_array[size + 2] = 0;
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


guint64 *get_lru_hit_count_seq_shards(reader_t *reader,
                                      gint64 size,
                                      double sample_ratio) {
  /* same as get_lru_hit_count_seq, but using shards generated data,
   * get the hit count, if size==-1, then do all the counting, otherwise,
   * treat the ones with reuse distance larger than size as out of range,
   * and put it in the second to the last bucket of hit_count_array
   * in other words, 0~size(included) are for counting REUSE_DIST=0~size-1, size+1 is
   * out of range, size+2 is cold miss, so total is size+3 buckets
   */


  guint64 ts = 0;
  gint64 reuse_dist;
  guint64 *hit_count_array = NULL;
  if (reader->base->n_total_req == -1)
    get_num_of_req(reader);

  if (size == -1)
    size = reader->base->n_total_req;

  assert(size == reader->base->n_total_req);

  /* for a cache_size=size, we need size+1 bucket for size 0~size(included),
   * the last element(size+1) is used for storing count of reuse distance > size
   * if size==reader->base->n_total_req, then the last two are not used
   */
  hit_count_array = g_new0(guint64, size + 3);
  assert(hit_count_array != NULL);

  request_t *req = new_request(reader->base->obj_id_type);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else {
    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }

  // TODO: when it is not run_trace with size, just use reuse distance for calculation,
  // because reuse distance might be loaded without re-computation

  // create splay tree
  sTree *splay_tree = NULL;
  gint64 num_reference_count = 0;
  gint64 num_unique_references = 0;
  read_one_req(reader, req);
  while (req->valid) {
    gint64 mod_val = 0xffffffff;
    gint64 mod_limit = mod_val * sample_ratio;

    uint32_t hash[1];                /* Output for the hash */
    uint32_t seed = 43;              /* Seed value for hash */

    MurmurHash3_x86_32(req->obj_id_ptr, strlen(req->obj_id_ptr), seed, hash);

    uint32_t hash_val = hash[0];
    gint64 mod_res = hash_val % mod_val;

    if (mod_res < mod_limit) {
      num_reference_count++;
      splay_tree = get_reuse_dist_add_req_shards(req, splay_tree, hash_table, ts, &reuse_dist, sample_ratio);
      assert(reuse_dist >= -1);
      if (reuse_dist == -1) {
        num_unique_references++;
        hit_count_array[size + 2] += 1;
      } else {
        reuse_dist = (gint64) (reuse_dist / sample_ratio);
        if (reuse_dist >= (size))
          hit_count_array[size + 1] += 1;
        else
          hit_count_array[reuse_dist] += 1;
      }
      ts++;
    }
    read_one_req(reader, req);
  }


  int expected_number_references = (int) floor(reader->base->n_total_req * sample_ratio);
  int diff_references = expected_number_references - num_reference_count;

  hit_count_array[0] += diff_references;


  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return hit_count_array;
}


double *get_lru_hit_ratio_seq_shards(reader_t *reader,
                                     gint64 size,
                                     double sample_ratio) {

  if (reader->base->n_total_req == -1)
    reader->base->n_total_req = get_num_of_req(reader);

  if (size == -1)
    size = reader->base->n_total_req;

  assert(size == reader->base->n_total_req);

//  if (reader->udata->hit_ratio_shards && size == reader->base->n_total_req)
//    return reader->udata->hit_ratio_shards;


  guint64 *hit_count_array = get_lru_hit_count_seq_shards(reader, size, sample_ratio);
  assert(hit_count_array != NULL);
  double total_num = (double) (reader->base->n_total_req * sample_ratio);
  double *hit_ratio_array = g_new0(double, size + 3);

  hit_ratio_array[0] = hit_count_array[0] / total_num;
  size_t i = 0;
  for (i = 1; i < size + 1; i++) {
    hit_ratio_array[i] = hit_count_array[i] / total_num + hit_ratio_array[i - 1];
  }
  // larger than given cache size
//  hit_ratio_array[size + 1] = hit_count_array[size + 1] / n_total_req;
  // cold miss
//  hit_ratio_array[size + 2] = hit_count_array[size + 2] / n_total_req;

  assert(hit_count_array != NULL);

  g_free(hit_count_array);

//  if (size == reader->base->n_total_req)
//    reader->udata->hit_ratio_shards = hit_ratio_array;

  return hit_ratio_array;
}

guint64 *get_lru_hit_count_phase(reader_t *reader, gint64 current_phase, gint64 num_phases) {

  guint64 ts = 0;
  gint64 reuse_dist;

  if (reader->base->n_total_req == -1)
    reader->base->n_total_req = get_num_of_req(reader);

  gint64 total_request = reader->base->n_total_req;
  double request_per_phase = (double)(total_request) / (double)(num_phases);
  gint64 start_request = (gint64) request_per_phase * current_phase;
  gint64 end_request = (gint64) (current_phase + 1) * request_per_phase - 1;
  gint64 size = (gint64) request_per_phase;
  guint64 *hit_count_array = g_new0(guint64, (gint64) request_per_phase + 3);

  
  request_t *req = new_request(reader->base->obj_id_type);

  // create hashtable
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, \
//                                           (GDestroyNotify) g_free,
                                       (GDestroyNotify) g_free);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                           (GDestroyNotify) g_free, \
                                           (GDestroyNotify) g_free);
  } else {
    ERROR("does not recognize reader data obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }

  // TODO: when it is not run_trace with size, just use reuse distance for calculation,
  // because reuse distance might be loaded without re-computation

  // create splay tree
  sTree *splay_tree = NULL;
  read_one_req(reader, req);
  gint64 request_count = 1;
  while (req->valid) {
    if (request_count >= start_request && request_count <= end_request) {
      splay_tree = get_reuse_dist_add_req(req, splay_tree, hash_table, ts, &reuse_dist);
      if (reuse_dist == -1)
        hit_count_array[size + 2] += 1;
      else if (reuse_dist >= size)
        hit_count_array[size + 1] += 1;
      else
        /* why + 1 here ? */
        hit_count_array[reuse_dist + 1] += 1;
    } else if (request_count > end_request)
      break;
    read_one_req(reader, req);
    request_count++;
    ts++;
  }

  // clean up
  free_request(req);
  g_hash_table_destroy(hash_table);
  free_sTree(splay_tree);
  reset_reader(reader);
  return hit_count_array;
}

double *get_lru_hit_ratio_phase(reader_t *reader, gint64 current_phase, gint64 num_phases) {

  if (reader->base->n_total_req == -1)
    reader->base->n_total_req = get_num_of_req(reader);

  gint64 total_request = reader->base->n_total_req;
  double request_per_phase = floor((double) total_request / num_phases);
  gint64 size = (gint64) request_per_phase;

  guint64 *hit_count_array = get_lru_hit_count_phase(reader, current_phase, num_phases);
  double *hit_ratio_array = g_new(double, size + 3);
  hit_ratio_array[0] = (double)(hit_count_array[0]) / (double) request_per_phase;

  for (gint64 i = 1; i < size + 1; i++) {
    hit_ratio_array[i] = hit_count_array[i] / request_per_phase + hit_ratio_array[i - 1];
  }

  // larger than given cache size
//  hit_ratio_array[size + 1] = hit_count_array[size + 1] / request_per_phase;
  // cold miss
//  hit_ratio_array[size + 2] = hit_count_array[size + 2] / request_per_phase;

  g_free(hit_count_array);

  return hit_ratio_array;
}

double *get_lru_hit_ratio_seq(reader_t *reader, gint64 size) {
  gint64 i = 0;
  if (reader->base->n_total_req == -1)
    reader->base->n_total_req = get_num_of_req(reader);

  if (size == -1)
    size = reader->base->n_total_req;

  guint64 *hit_count_array = get_lru_hit_count_seq(reader, size);
  double total_num = (double) (reader->base->n_total_req);

  double *hit_ratio_array = g_new(double, size + 3);
  hit_ratio_array[0] = hit_count_array[0] / total_num;
  for (i = 1; i < size + 1; i++) {
    hit_ratio_array[i] = hit_count_array[i] / total_num;
//    hit_ratio_array[i] = hit_count_array[i] / total_num + hit_ratio_array[-1];
  }

  // larger than given cache size
//  hit_ratio_array[size + 1] = hit_count_array[size + 1] / n_total_req;
  // cold miss
//  hit_ratio_array[size + 2] = hit_count_array[size + 2] / n_total_req;

  g_free(hit_count_array);
  return hit_ratio_array;
}






#ifdef __reqlusplus
}
#endif

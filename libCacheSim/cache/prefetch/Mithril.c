//
//  a Mithril module that supports different obj size
//
//
//  Mithril.c
//  libCacheSim
//
//  Created by Zhelong on 23/8/15.
//  Copyright © 2023 Zhelong. All rights reserved.
//
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "../../include/libCacheSim/prefetchAlgo.h"
#include "../../include/libCacheSim/prefetchAlgo/Mithril.h"
#include "glibconfig.h"

#define TRACK_BLOCK 192618l
#define SANITY_CHECK 1
#define PROFILING
// #define debug

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****               helper function declarations                    ****
// ****                                                               ****
// ***********************************************************************
static inline bool _Mithril_check_sequential(cache_t *Mithril,
                                             const request_t *req);
static inline void _Mithril_record_entry(cache_t *Mithril,
                                         const request_t *req);
static inline void _Mithril_rec_min_support_one(cache_t *Mithril,
                                                const request_t *req);
static inline gint _Mithril_get_total_num_of_ts(gint64 *row, gint row_length);
static void _Mithril_mining(cache_t *Mithril);

static void _Mithril_add_to_prefetch_table(cache_t *Mithril, gpointer gp1,
                                           gpointer gp2);

const char *Mithril_default_params(void) {
  return "lookahead-range=20, "
         "max-support=8, min-support=2, confidence=1, pf-list-size=2, "
         "rec-trigger=miss, block-size=1, max-metadata-size=0.1, "
         "cycle-time=2, mining-threshold=5120, sequential-type=0, "
         "sequential-K=-1, AMP-pthreshold=-1";
}

static void set_Mithril_default_init_params(
    Mithril_init_params_t *init_params) {
  init_params->lookahead_range = 20;
  init_params->max_support = 8;
  init_params->min_support = 2;
  init_params->confidence = 1;
  init_params->pf_list_size = 2;
  init_params->rec_trigger = miss;
  init_params->block_size = 1;  // for general use
  init_params->max_metadata_size = 0.1;
  init_params->cycle_time = 2;
  init_params->mining_threshold = MINING_THRESHOLD;

  init_params->sequential_type = 0;
  init_params->sequential_K = -1;

  init_params->AMP_pthreshold = -1;
}

static void Mithril_parse_init_params(const char *cache_specific_params,
                                      Mithril_init_params_t *init_params) {
  char *params_str = strdup(cache_specific_params);

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }
    if (strcasecmp(key, "lookahead-range") == 0) {
      init_params->lookahead_range = atoi(value);
    } else if (strcasecmp(key, "max-support") == 0) {
      init_params->max_support = atoi(value);
    } else if (strcasecmp(key, "min-support") == 0) {
      init_params->min_support = atoi(value);
    } else if (strcasecmp(key, "confidence") == 0) {
      init_params->confidence = atoi(value);
    } else if (strcasecmp(key, "pf-list-size") == 0) {
      init_params->pf_list_size = atoi(value);
    } else if (strcasecmp(key, "rec-trigger") == 0) {
      if (strcasecmp(value, "miss") == 0) {
        init_params->rec_trigger = miss;
      } else if (strcasecmp(value, "evict") == 0) {
        init_params->rec_trigger = evict;
      } else if (strcasecmp(value, "miss_evict") == 0) {
        init_params->rec_trigger = miss_evict;
      } else if (strcasecmp(value, "each_req") == 0) {
        init_params->rec_trigger = each_req;
      } else {
        ERROR("Mithril's rec-trigger does not support %s \n", value);
      }
    } else if (strcasecmp(key, "block-size") == 0) {
      init_params->block_size = (unsigned long)atoi(value);
    } else if (strcasecmp(key, "max-metadata-size") == 0) {
      init_params->max_metadata_size = atof(value);
    } else if (strcasecmp(key, "cycle-time") == 0) {
      init_params->cycle_time = atoi(value);
    } else if (strcasecmp(key, "mining-threshold") == 0) {
      init_params->mining_threshold = atoi(value);
    } else if (strcasecmp(key, "sequential-type") == 0) {
      init_params->sequential_type = atoi(value);
    } else if (strcasecmp(key, "sequential-K") == 0) {
      init_params->sequential_K = atoi(value);
    } else if (strcasecmp(key, "AMP-pthreshold") == 0) {
      init_params->AMP_pthreshold = atoi(value);
    } else if (strcasecmp(key, "print") == 0 ||
               strcasecmp(key, "default") == 0) {
      printf("default params: %s\n", Mithril_default_params());
      exit(0);
    } else {
      ERROR("Mithril does not have parameter %s\n", key);
      printf("default params: %s\n", Mithril_default_params());
      exit(1);
    }
  }
}

static void set_Mithril_params(Mithril_params_t *Mithril_params,
                               Mithril_init_params_t *init_params,
                               uint64_t cache_size) {
  Mithril_params->lookahead_range = init_params->lookahead_range;
  Mithril_params->max_support = init_params->max_support;
  Mithril_params->min_support = init_params->min_support;
  Mithril_params->confidence = init_params->confidence;
  Mithril_params->cycle_time = init_params->cycle_time;
  Mithril_params->pf_list_size = init_params->pf_list_size;
  Mithril_params->mining_threshold = init_params->mining_threshold;

  Mithril_params->block_size = init_params->block_size;
  Mithril_params->sequential_type = init_params->sequential_type;
  Mithril_params->sequential_K = init_params->sequential_K;
  Mithril_params->output_statistics = 1;

  Mithril_params->mtable_size =
      (gint)(init_params->mining_threshold / Mithril_params->min_support);

  Mithril_params->rec_trigger = init_params->rec_trigger;

  Mithril_params->max_metadata_size =
      (gint64)(init_params->block_size * cache_size *
               init_params->max_metadata_size);

  gint max_num_of_shards_in_prefetch_table =
      (gint)(Mithril_params->max_metadata_size /
             (PREFETCH_TABLE_SHARD_SIZE * init_params->pf_list_size));
  assert(max_num_of_shards_in_prefetch_table > 0);
  /* now adjust the cache size by deducting current meta data size
   8 is the size of storage for block, 4 is the size of storage for index to
   array */
  Mithril_params->cur_metadata_size =
      (init_params->max_support * 2 + 8 + 4) * Mithril_params->mtable_size +
      max_num_of_shards_in_prefetch_table * 8 +
      PREFETCH_TABLE_SHARD_SIZE * (Mithril_params->pf_list_size * 8 + 8 + 4);

  Mithril_params->rmtable = g_new0(rec_mining_t, 1);
  rec_mining_t *rmtable = Mithril_params->rmtable;
  rmtable->n_avail_mining = 0;
  rmtable->rtable_cur_row = 1;
  rmtable->rtable_row_len =
      (gint)ceil((double)Mithril_params->min_support / (double)4) + 1;
  rmtable->mtable_row_len =
      (gint)ceil((double)Mithril_params->max_support / (double)4) + 1;
  rmtable->mining_table =
      g_array_sized_new(FALSE, TRUE, sizeof(int64_t) * rmtable->mtable_row_len,
                        Mithril_params->mtable_size);
  rmtable->hashtable =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  Mithril_params->prefetch_hashtable =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  Mithril_params->cache_size_map =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

  if (Mithril_params->output_statistics) {
    Mithril_params->prefetched_hashtable_Mithril =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    Mithril_params->prefetched_hashtable_sequential =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  }

  Mithril_params->ptable_cur_row = 1;
  Mithril_params->ptable_is_full = FALSE;
  // always save to size+1 position, and enlarge table when size%shards_size ==
  // 0
  Mithril_params->ptable_array =
      g_new0(gint64 *, max_num_of_shards_in_prefetch_table);
  Mithril_params->ptable_array[0] = g_new0(
      gint64, PREFETCH_TABLE_SHARD_SIZE * (Mithril_params->pf_list_size + 1));

  Mithril_params->ts = 0;

  Mithril_params->hit_on_prefetch_Mithril = 0;
  Mithril_params->hit_on_prefetch_sequential = 0;
  Mithril_params->num_of_prefetch_Mithril = 0;
  Mithril_params->num_of_prefetch_sequential = 0;
  Mithril_params->num_of_check = 0;

  if (Mithril_params->max_support != 1) {
    rmtable->n_rows_in_rtable =
        (gint64)(cache_size * Mithril_params->block_size *
                 RECORDING_TABLE_MAXIMAL /
                 ((int)ceil((double)Mithril_params->min_support / (double)2) *
                      2 +
                  8 + 4));
    rmtable->recording_table = g_new0(
        gint64, rmtable->n_rows_in_rtable *
                    rmtable->rtable_row_len);  // this should begins with 1
    Mithril_params->cur_metadata_size +=
        (((gint64)ceil((double)init_params->min_support / (double)4 + 1) * 8 +
          4) *
         rmtable->n_rows_in_rtable);
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                     prefetcher interfaces                     ****
// ****                                                               ****
// ****   create, free, clone, handle_find, handle_evict, prefetch    ****
// ***********************************************************************
/**
 1. record the request in cache_size_map for being aware of prefetching object's
 size in the future.
 2. record entry if rec_trigger is not evict.

 @param cache the cache struct
 @param req the request containing the request
 @return
*/
static void Mithril_handle_find(cache_t *cache, const request_t *req,
                                bool hit) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);

  /*use cache_size_map to record the current requested obj's size*/
  g_hash_table_insert(Mithril_params->cache_size_map,
                      GINT_TO_POINTER(req->obj_id),
                      GINT_TO_POINTER(req->obj_size));

  if (Mithril_params->output_statistics) {
    if (g_hash_table_contains(Mithril_params->prefetched_hashtable_Mithril,
                              GINT_TO_POINTER(req->obj_id))) {
      Mithril_params->hit_on_prefetch_Mithril += 1;
      g_hash_table_remove(Mithril_params->prefetched_hashtable_Mithril,
                          GINT_TO_POINTER(req->obj_id));
    }
    if (g_hash_table_contains(Mithril_params->prefetched_hashtable_sequential,
                              GINT_TO_POINTER(req->obj_id))) {
      Mithril_params->hit_on_prefetch_sequential += 1;
      g_hash_table_remove(Mithril_params->prefetched_hashtable_sequential,
                          GINT_TO_POINTER(req->obj_id));
    }
  }

  // 1. record entry when rec_trigger is each_req.
  // 2. record entry when (rec_trigger is miss or miss_evict (in other words,
  // !evict)) && !hit
  if ((Mithril_params->rec_trigger == each_req) ||
      (Mithril_params->rec_trigger != evict && !hit)) {
    _Mithril_record_entry(cache, req);
  }
}

/**
 evict_req->obj_id has been evict by cache_remove_base.
 Now, prefetcher checks whether it can be added to cache (second chance).

 @param cache the cache struct
 @param req the request containing the request
 @return
*/
void Mithril_handle_evict(cache_t *cache, const request_t *check_req) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);

  if (Mithril_params->output_statistics) {
    obj_id_t check_id = check_req->obj_id;

    gint type = GPOINTER_TO_INT(
        g_hash_table_lookup(Mithril_params->prefetched_hashtable_Mithril,
                            GINT_TO_POINTER(check_id)));
    if (type != 0 && type < Mithril_params->cycle_time) {
      // give one more chance
      g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril,
                          GINT_TO_POINTER(check_id), GINT_TO_POINTER(type + 1));

      while ((long)cache->get_occupied_byte(cache) + check_req->obj_size +
                 cache->obj_md_size >
             (long)cache->cache_size) {
        cache->evict(cache, check_req);
      }
      cache->insert(cache, check_req);
    } else {
      if (Mithril_params->rec_trigger == evict ||
          Mithril_params->rec_trigger == miss_evict) {
        _Mithril_record_entry(cache, check_req);
      }

      g_hash_table_remove(Mithril_params->prefetched_hashtable_Mithril,
                          GINT_TO_POINTER(check_req->obj_id));
      g_hash_table_remove(Mithril_params->prefetched_hashtable_sequential,
                          GINT_TO_POINTER(check_req->obj_id));
    }
  }
}

/**
 prefetch some objs associated with req->obj_id by searching prefetch_hashtable
 and ptable_array and evict when space is full.

 @param cache the cache struct
 @param req the request containing the request
 @return
 */
void Mithril_prefetch(cache_t *cache, const request_t *req) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);

  gint prefetch_table_index = GPOINTER_TO_INT(g_hash_table_lookup(
      Mithril_params->prefetch_hashtable, GINT_TO_POINTER(req->obj_id)));

  gint dim1 =
      (gint)floor(prefetch_table_index / (double)PREFETCH_TABLE_SHARD_SIZE);
  gint dim2 = prefetch_table_index % PREFETCH_TABLE_SHARD_SIZE *
              (Mithril_params->pf_list_size + 1);

  request_t *new_req = my_malloc(request_t);
  copy_request(new_req, req);

  if (prefetch_table_index) {
    int i;
    for (i = 1; i < Mithril_params->pf_list_size + 1; i++) {
      // begin from 1 because index 0 is the obj_id of originated request
      if (Mithril_params->ptable_array[dim1][dim2 + i] == 0) {
        break;
      }
      new_req->obj_id = Mithril_params->ptable_array[dim1][dim2 + i];
      new_req->obj_size = GPOINTER_TO_INT(g_hash_table_lookup(
          Mithril_params->cache_size_map, GINT_TO_POINTER(new_req->obj_id)));

      if (Mithril_params->output_statistics) {
        Mithril_params->num_of_check += 1;
      }

      if (cache->find(cache, new_req, false)) {
        continue;
      }

      while ((long)cache->get_occupied_byte(cache) + new_req->obj_size +
                 cache->obj_md_size >
             (long)cache->cache_size) {
        cache->evict(cache, new_req);
      }
      cache->insert(cache, new_req);

      if (Mithril_params->output_statistics) {
        Mithril_params->num_of_prefetch_Mithril += 1;

        g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril,
                            GINT_TO_POINTER(new_req->obj_id),
                            GINT_TO_POINTER(1));
      }
    }
  }

  // prefetch sequential
  // just use in block or cache line level where obj_size is same
  if (Mithril_params->sequential_type == 1 &&
      _Mithril_check_sequential(cache, req)) {
    new_req->obj_id = req->obj_id + 1;
    new_req->obj_size = req->obj_size;  // same size

    if (cache->find(cache, new_req, false)) {
      my_free(sizeof(request_t), new_req);
      return;
    }

    // use this, not add because we need to record stat when evicting

    while ((long)cache->get_occupied_byte(cache) + new_req->obj_size +
               cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, new_req);
    }
    cache->insert(cache, new_req);

    if (Mithril_params->output_statistics) {
      Mithril_params->num_of_prefetch_sequential += 1;
      g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril,
                          GINT_TO_POINTER(new_req->obj_id), GINT_TO_POINTER(1));
    }
  }
  my_free(sizeof(request), new_req);

  Mithril_params->ts++;
}

void free_Mithril_prefetcher(prefetcher_t *prefetcher) {
  Mithril_params_t *Mithril_params = (Mithril_params_t *)prefetcher->params;

  g_hash_table_destroy(Mithril_params->prefetch_hashtable);
  g_hash_table_destroy(Mithril_params->cache_size_map);
  g_hash_table_destroy(Mithril_params->rmtable->hashtable);
  g_free(Mithril_params->rmtable->recording_table);
  g_array_free(Mithril_params->rmtable->mining_table, TRUE);
  g_free(Mithril_params->rmtable);

  int i = 0;
  gint max_num_of_shards_in_prefetch_table =
      (gint)(Mithril_params->max_metadata_size /
             (PREFETCH_TABLE_SHARD_SIZE * Mithril_params->pf_list_size));

  while (i < max_num_of_shards_in_prefetch_table) {
    if (Mithril_params->ptable_array[i]) {
      g_free(Mithril_params->ptable_array[i]);
    } else {
      break;
    }
    i++;
  }
  g_free(Mithril_params->ptable_array);

  if (Mithril_params->output_statistics) {
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_Mithril);
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_sequential);
  }
  my_free(sizeof(Mithril_params_t), Mithril_params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  my_free(sizeof(prefetcher_t), prefetcher);
}

prefetcher_t *clone_Mithril_prefetcher(prefetcher_t *prefetcher,
                                       uint64_t cache_size) {
  return create_Mithril_prefetcher(prefetcher->init_params, cache_size);
}

prefetcher_t *create_Mithril_prefetcher(const char *init_params,
                                        uint64_t cache_size) {
  Mithril_init_params_t *Mithril_init_params = my_malloc(Mithril_init_params_t);
  memset(Mithril_init_params, 0, sizeof(Mithril_init_params_t));

  set_Mithril_default_init_params(Mithril_init_params);
  if (init_params != NULL) {
    Mithril_parse_init_params(init_params, Mithril_init_params);
    check_params((Mithril_init_params));
  }

  Mithril_params_t *Mithril_params = my_malloc(Mithril_params_t);
  // when all object's size is 1, cache->cache_size is the number of objects
  // that can be cached, and users should set block_size in prefetching_params.
  // Otherwise, cache->cache_size is the total bytes that can be cached and
  // block_size is 1 in the default setting.
  set_Mithril_params(Mithril_params, Mithril_init_params, cache_size);

  prefetcher_t *prefetcher = (prefetcher_t *)my_malloc(prefetcher_t);
  memset(prefetcher, 0, sizeof(prefetcher_t));
  prefetcher->params = Mithril_params;
  prefetcher->prefetch = Mithril_prefetch;
  prefetcher->handle_find = Mithril_handle_find;
  prefetcher->handle_evict = Mithril_handle_evict;
  prefetcher->free = free_Mithril_prefetcher;
  prefetcher->clone = clone_Mithril_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  my_free(sizeof(Mithril_init_params_t), Mithril_init_params);
  return prefetcher;
}

/******************** Mithril help function ********************/
/**
 check whether last request is part of a sequential access
 */
static inline bool _Mithril_check_sequential(cache_t *cache,
                                             const request_t *req) {
  int i;
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);
  if (Mithril_params->sequential_K == 0) return FALSE;

  request_t *new_req = my_malloc(request_t);
  copy_request(new_req, req);
  bool is_sequential = TRUE;
  gint sequential_K = Mithril_params->sequential_K;
  if (sequential_K == -1) { /* when use AMP, this is -1 */
    sequential_K = 1;
  }
  for (i = 0; i < sequential_K; i++) {
    new_req->obj_id--;
    if (!cache->find(cache, new_req, false)) {
      is_sequential = FALSE;
      break;
    }
  }
  return is_sequential;
}

static inline void _Mithril_rec_min_support_one(cache_t *cache,
                                                const request_t *req) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);
  rec_mining_t *rmtable = Mithril_params->rmtable;

#ifdef TRACK_BLOCK
  if (req->obj_id == TRACK_BLOCK) {
    int old_pos = GPOINTER_TO_INT(
        g_hash_table_lookup(rmtable->hashtable, GINT_TO_POINTER(req->obj_id)));
    printf("insert %ld, old pos %d", TRACK_BLOCK, old_pos);
    if (old_pos == 0)
      printf("\n");
    else
      printf(", block at old_pos %ld\n",
             (long)*(gint64 *)GET_ROW_IN_MTABLE(Mithril_params, old_pos - 1));

  } else {
    gint64 b = TRACK_BLOCK;
    int old_pos = GPOINTER_TO_INT(
        g_hash_table_lookup(rmtable->hashtable, GINT_TO_POINTER(b)));
    if (old_pos != 0) {
      ERROR("ts %lu, checking %ld, %ld is found at pos %d\n",
            (unsigned long)Mithril_params->ts, (long)TRACK_BLOCK,
            (long)*(gint64 *)GET_ROW_IN_MTABLE(Mithril_params, old_pos - 1),
            old_pos);
      abort();
    }
  }
#endif

  int i;
  // check the obj_id in hashtable for training
  gint index = GPOINTER_TO_INT(
      g_hash_table_lookup(rmtable->hashtable, GINT_TO_POINTER(req->obj_id)));
  if (index == 0) {
    // the node is not in the recording/mining data, should be added
    gint64 array_ele[rmtable->mtable_row_len];
    // gpointer hash_key;
    array_ele[0] = req->obj_id;
    // hash_key = GET_ROW_IN_MTABLE(Mithril_params,
    // rmtable->mining_table->len);

    for (i = 1; i < rmtable->mtable_row_len; i++) array_ele[i] = 0;
    array_ele[1] = ADD_TS(array_ele[1], Mithril_params->ts);

    g_array_append_val(rmtable->mining_table, array_ele);
    rmtable->n_avail_mining++;

    // all index is real row number + 1
    g_hash_table_insert(rmtable->hashtable, GINT_TO_POINTER(req->obj_id),
                        GINT_TO_POINTER(rmtable->mining_table->len));

#ifdef SANITY_CHECK
    gint64 *row_in_mtable =
        GET_ROW_IN_MTABLE(Mithril_params, rmtable->mining_table->len - 1);
    if (req->obj_id != row_in_mtable[0]) {
      ERROR("after inserting, hashtable mining not consistent %ld %ld\n",
            (long)req->obj_id, (long)row_in_mtable[0]);
      abort();
    }
#endif
  } else {
    /* in mining table */
    gint64 *row_in_mtable = GET_ROW_IN_MTABLE(Mithril_params, index - 1);

#ifdef SANITY_CHECK
    if (req->obj_id != row_in_mtable[0]) {
      ERROR("ts %lu, hashtable mining found position not correct %ld %ld\n",
            (unsigned long)Mithril_params->ts, (long)req->obj_id,
            (long)row_in_mtable[0]);
      abort();
    }
#endif

    int timestamps_length = 0;

    for (i = 1; i < rmtable->mtable_row_len; i++) {
      timestamps_length += NUM_OF_TS(row_in_mtable[i]);
      if (NUM_OF_TS(row_in_mtable[i]) < 4) {
        row_in_mtable[i] = ADD_TS(row_in_mtable[i], Mithril_params->ts);
        break;
      }
    }
    if (timestamps_length == Mithril_params->max_support) {
      /* no timestamp added, drop this request, it is too frequent */
      if (!g_hash_table_remove(rmtable->hashtable,
                               GINT_TO_POINTER(row_in_mtable[0]))) {
        ERROR("removing from rmtable failed for mining table entry\n");
      }

      g_array_remove_index_fast(rmtable->mining_table, index - 1);

      // if array is moved, need to update hashtable
      if (index - 1 != (long)rmtable->mining_table->len) {
        g_hash_table_replace(rmtable->hashtable,
                             GINT_TO_POINTER(row_in_mtable[0]),
                             GINT_TO_POINTER(index));
      }
      rmtable->n_avail_mining--;
    }
  }
}

/**
 record req to the recording table or the mining table

 @param Mithril the cache struct
 @param req the request containing the request
 @return
 */
static inline void _Mithril_record_entry(cache_t *cache, const request_t *req) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);
  rec_mining_t *rmtable = Mithril_params->rmtable;

  int i;

  /* check it is sequential or not */
  if (Mithril_params->sequential_type && _Mithril_check_sequential(cache, req))
    return;

  if (Mithril_params->min_support == 1) {
    _Mithril_rec_min_support_one(cache, req);
  } else {
    gint64 *row_in_rtable;
    // check the obj_id in hashtable for training
    gint index = GPOINTER_TO_INT(
        g_hash_table_lookup(rmtable->hashtable, GINT_TO_POINTER(req->obj_id)));

    if (index == 0) {
      // the node is not in the recording/mining data, should be added
      row_in_rtable = GET_CUR_ROW_IN_RTABLE(Mithril_params);

#ifdef SANITY_CHECK
      if (row_in_rtable[0] != 0) {
        ERROR("recording table is not clean\n");
        abort();
      }
#endif

      row_in_rtable[0] = req->obj_id;
      // row_in_rtable is a pointer to the block number
      g_hash_table_insert(rmtable->hashtable, GINT_TO_POINTER(row_in_rtable[0]),
                          GINT_TO_POINTER(rmtable->rtable_cur_row));

      row_in_rtable[1] = ADD_TS(row_in_rtable[1], Mithril_params->ts);

      // move cur_row to next
      rmtable->rtable_cur_row++;
      if (rmtable->rtable_cur_row >= rmtable->n_rows_in_rtable) {
        /* recording table is full */
        rmtable->rtable_cur_row = 1;
      }

      row_in_rtable =
          GET_ROW_IN_RTABLE(Mithril_params, rmtable->rtable_cur_row);

      if (row_in_rtable[0] != 0) {
        /** clear current row,
         *  this is because the recording table is full
         *  and we need to begin from beginning
         *  and current position has old resident,
         *  we need to remove them
         **/
        if (!g_hash_table_contains(rmtable->hashtable,
                                   GINT_TO_POINTER(row_in_rtable[0]))) {
          ERROR(
              "remove old entry from recording table, "
              "but it is not in recording hashtable, "
              "block %ld, recording table pos %ld, ts %ld ",
              (long)row_in_rtable[0], (long)rmtable->rtable_cur_row,
              (long)Mithril_params->ts);

          long temp = rmtable->rtable_cur_row - 1;
          fprintf(stderr, "previous line block %ld\n",
                  *(long *)(GET_ROW_IN_RTABLE(Mithril_params, temp)));
          abort();
        }

        g_hash_table_remove(rmtable->hashtable,
                            GINT_TO_POINTER(row_in_rtable[0]));

        /* clear recording table */
        for (i = 0; i < rmtable->rtable_row_len; i++) {
          row_in_rtable[i] = 0;
        }
      }
    } else {
      /** first check it is in recording table or mining table,
       *  if in mining table (index < 0),
       *  check how many ts it has, if equal max_support, remove it
       *  otherwise add to mining table;
       *  if in recording table (index > 0),
       *  check how many ts it has ,
       *  if equal to min_support-1, add and move to mining table,
       **/
      if (index < 0) {
        /* in mining table */
        gint64 *row_in_mtable = GET_ROW_IN_MTABLE(Mithril_params, -index - 1);

#ifdef SANITY_CHECK
        if (req->obj_id != row_in_mtable[0]) {
          ERROR(
              "inconsistent entry in mtable "
              "and mining hashtable current request %ld, "
              "mining table %ld\n",
              (long)req->obj_id, (long)row_in_mtable[0]);
          abort();
        }
#endif
        int timestamps_length = 0;

        for (i = 1; i < rmtable->mtable_row_len; i++) {
          timestamps_length += NUM_OF_TS(row_in_mtable[i]);
          if (NUM_OF_TS(row_in_mtable[i]) < 4) {
            row_in_mtable[i] = ADD_TS(row_in_mtable[i], Mithril_params->ts);
            break;
          }
        }
        if (timestamps_length == Mithril_params->max_support) {
          /* no timestamp added, drop this request, it is too frequent */
          if (!g_hash_table_remove(rmtable->hashtable,
                                   GINT_TO_POINTER(row_in_mtable[0]))) {
            ERROR("removing from rmtable failed for mining table entry\n");
          }

          /** for dataType c, now the pointer to string has been freed,
           *  so mining table entry is incorrect,
           *  but mining table entry will be deleted, so it is OK
           */

          g_array_remove_index_fast(rmtable->mining_table, -index - 1);

          /** if the removed block is not the last entry,
           *  g_array_remove_index_fast uses the last entry to fill in
           *  the old position, so we need to update its index
           **/
          if (-index - 1 != (long)rmtable->mining_table->len) {
            g_hash_table_replace(rmtable->hashtable,
                                 GINT_TO_POINTER(row_in_mtable[0]),
                                 GINT_TO_POINTER(index));
          }
          rmtable->n_avail_mining--;
        }
      } else {
        /* in recording table */
        row_in_rtable = GET_ROW_IN_RTABLE(Mithril_params, index);
        gint64 *cur_row_in_rtable =
            GET_ROW_IN_RTABLE(Mithril_params, rmtable->rtable_cur_row - 1);
        int timestamps_length = 0;

#ifdef SANITY_CHECK
        if (req->obj_id != row_in_rtable[0]) {
          ERROR("Hashtable recording found position not correct %ld %ld\n",
                (long)req->obj_id, (long) row_in_rtable[0]);
          abort();
        }
#endif

        for (i = 1; i < rmtable->rtable_row_len; i++) {
          timestamps_length += NUM_OF_TS(row_in_rtable[i]);
          if (NUM_OF_TS(row_in_rtable[i]) < 4) {
            row_in_rtable[i] = ADD_TS(row_in_rtable[i], Mithril_params->ts);
            break;
          }
        }

        if (timestamps_length == Mithril_params->min_support - 1) {
          /* time to move to mining table */
          // gint64 *array_ele = malloc(sizeof(gint64) *
          // rmtable->mtable_row_len);
          gint64 array_ele[rmtable->mtable_row_len];
          memcpy(array_ele, row_in_rtable,
                 sizeof(TS_REPRESENTATION) * rmtable->rtable_row_len);

          /** clear the rest of array,
           *  this is important as
           *  we don't clear the content of array after mining
           **/
          memset(array_ele + rmtable->rtable_row_len, 0,
                 sizeof(TS_REPRESENTATION) *
                     (rmtable->mtable_row_len - rmtable->rtable_row_len));
#ifdef SANITY_CHECK
          if ((long)rmtable->mining_table->len >= Mithril_params->mtable_size) {
            /* if this happens, array will re-malloc, which will make
             * the hashtable key not reliable when obj_id_type is l */
            ERROR(
                "mining table length reaches limit, but no mining, "
                "entry %d, size %u, threshold %d\n",
                rmtable->n_avail_mining, rmtable->mining_table->len,
                Mithril_params->mtable_size);
            abort();
          }
#endif
          g_array_append_val(rmtable->mining_table, array_ele);
          rmtable->n_avail_mining++;

          if (index != rmtable->rtable_cur_row - 1 &&
              rmtable->rtable_cur_row >= 2) {
            /** moved row is not the last entry in recording table
             *  move last row to current position
             **/

#ifdef SANITY_CHECK
            if (row_in_rtable == cur_row_in_rtable)
              ERROR("FOUND SRC DEST same, ts %ld %p %p %ld %ld %d %ld\n",
                    (long)Mithril_params->ts, row_in_rtable, cur_row_in_rtable,
                    (long)*row_in_rtable, (long)*cur_row_in_rtable, index,
                    (long)rmtable->rtable_cur_row - 1);
#endif
            memcpy(row_in_rtable, cur_row_in_rtable,
                   sizeof(TS_REPRESENTATION) * rmtable->rtable_row_len);
          }
          if (rmtable->rtable_cur_row >= 2) {
            for (i = 0; i < rmtable->rtable_row_len; i++) {
              cur_row_in_rtable[i] = 0;
            }
          } else {
            /** if current pointer points to 1,
             *  then don't move it, clear the row (that moves to mining table)
             **/
            for (i = 0; i < rmtable->rtable_row_len; i++) row_in_rtable[i] = 0;
          }

          gint64 *inserted_row_in_mtable =
              GET_ROW_IN_MTABLE(Mithril_params, rmtable->mining_table->len - 1);

#ifdef SANITY_CHECK
          if (inserted_row_in_mtable[0] != (gint64)req->obj_id) {
            ERROR("current block %ld, moving mining row block %ld\n",
                  (long)req->obj_id, (long)inserted_row_in_mtable[0]);
            abort();
          }
#endif
          /** because we don't want to have zero as index,
           *  so we add one before taking negative,
           *  in other words, the range of mining table index
           *  is -1 ~ -max_index-1, mapping to 0~max_index
           */
          g_hash_table_replace(
              rmtable->hashtable, GINT_TO_POINTER(inserted_row_in_mtable[0]),
              GINT_TO_POINTER(-((gint)rmtable->mining_table->len - 1 + 1)));

          if (index != rmtable->rtable_cur_row - 1 &&
              rmtable->rtable_cur_row >= 2)
            // last entry in the recording table is moved up index position
            g_hash_table_replace(rmtable->hashtable,
                                 GINT_TO_POINTER(row_in_rtable[0]),
                                 GINT_TO_POINTER(index));

          // one entry has been moved to mining table, shrinking recording
          // table size by 1
          if (rmtable->rtable_cur_row >= 2) rmtable->rtable_cur_row--;

          // free(array_ele);
        }
      }
    }
  }
  if (rmtable->n_avail_mining >= Mithril_params->mtable_size ||
      (Mithril_params->min_support == 1 &&
       rmtable->n_avail_mining > Mithril_params->mining_threshold / 8)) {
    _Mithril_mining(cache);
    rmtable->n_avail_mining = 0;
  }
}

static inline gint _Mithril_get_total_num_of_ts(gint64 *row, gint row_length) {
  int i, t;
  int count = 0;
  for (i = 1; i < row_length; i++) {
    t = NUM_OF_TS(row[i]);
    if (t == 0) return count;
    count += t;
  }
  return count;
}

gint mining_table_entry_cmp(gconstpointer a, gconstpointer b) {
  return (gint)GET_NTH_TS(a, 1) - (gint)GET_NTH_TS(b, 1);
}

/* in debug */
void print_one_line(gpointer key, gpointer value, gpointer user_data) {
  gint src_key = GPOINTER_TO_INT(key);
  gint prefetch_table_index = GPOINTER_TO_INT(value);
  Mithril_params_t *Mithril_params = (Mithril_params_t *)user_data;
  gint dim1 =
      (gint)floor(prefetch_table_index / (double)PREFETCH_TABLE_SHARD_SIZE);
  gint dim2 = prefetch_table_index % PREFETCH_TABLE_SHARD_SIZE *
              (Mithril_params->pf_list_size + 1);
  printf("src %d, prefetch ", src_key);
  for (int i = 1; i < Mithril_params->pf_list_size + 1; i++) {
    printf("%ld ", (long)Mithril_params->ptable_array[dim1][dim2 + i]);
  }
  printf("\n");
}

/* in debug */
void print_prefetch_table(Mithril_params_t *Mithril_params) {
  g_hash_table_foreach(Mithril_params->prefetch_hashtable, print_one_line,
                       Mithril_params);
}

/**
 the mining function, it is called when mining table is ready

 @param Mithril the cache struct
 */
static void _Mithril_mining(cache_t *cache) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);
  rec_mining_t *rmtable = Mithril_params->rmtable;

#ifdef PROFILING
  GTimer *timer = g_timer_new();
  gulong microsecond;
  g_timer_start(timer);
#endif

  int i, j, k;

  /* first sort mining table, then do the mining */
  /* first remove all elements from hashtable, otherwise after sort, it will
   mess up for obj_id_type l but we can't do this for dataType c, otherwise
   the string will be freed during remove in hashtable
   */
  gint64 *item = (gint64 *)rmtable->mining_table->data;
  for (i = 0; i < (int)rmtable->mining_table->len; i++) {
    g_hash_table_remove(rmtable->hashtable, GINT_TO_POINTER(*item));
    item += rmtable->mtable_row_len;
  }

  g_array_sort(rmtable->mining_table, mining_table_entry_cmp);

  gboolean associated_flag, first_flag;
  gint64 *item1, *item2;
  gint num_of_ts1, num_of_ts2, shorter_length;
  for (i = 0; i < (long)rmtable->mining_table->len - 1; i++) {
    item1 = GET_ROW_IN_MTABLE(Mithril_params, i);
    num_of_ts1 = _Mithril_get_total_num_of_ts(item1, rmtable->mtable_row_len);
    first_flag = TRUE;

    for (j = i + 1; j < (long)rmtable->mining_table->len; j++) {
      item2 = GET_ROW_IN_MTABLE(Mithril_params, j);

      // check first timestamp
      if (GET_NTH_TS(item2, 1) - GET_NTH_TS(item1, 1) >
          Mithril_params->lookahead_range) {
        break;
      }
      num_of_ts2 = _Mithril_get_total_num_of_ts(item2, rmtable->mtable_row_len);

      if (ABS(num_of_ts1 - num_of_ts2) > Mithril_params->confidence) {
        continue;
      }

      shorter_length = MIN(num_of_ts1, num_of_ts2);

      associated_flag = FALSE;
      if (first_flag) {
        associated_flag = TRUE;
        first_flag = FALSE;
      }
      // is next line useless??
      if (shorter_length == 1 &&
          ABS(GET_NTH_TS(item1, 1) - GET_NTH_TS(item2, 1)) == 1) {
        associated_flag = TRUE;
      }

      gint error = 0;
      for (k = 1; k < shorter_length; k++) {
        if (ABS(GET_NTH_TS(item1, k) - GET_NTH_TS(item2, k)) >
            Mithril_params->lookahead_range) {
          error++;
          if (error > Mithril_params->confidence) {
            associated_flag = FALSE;
            break;
          }
        }

        if (ABS(GET_NTH_TS(item1, k) - GET_NTH_TS(item2, k)) == 1) {
          associated_flag = TRUE;
        }
      }
      if (associated_flag) {
        // finally, add to prefetch table
        _Mithril_add_to_prefetch_table(cache, GINT_TO_POINTER(item1[0]),
                                       GINT_TO_POINTER(item2[0]));
      }
    }
  }

  // may be just following?
  rmtable->mining_table->len = 0;

#ifdef PROFILING
  printf("ts: %lu, clearing training data takes %lf seconds\n",
         (unsigned long)Mithril_params->ts,
         g_timer_elapsed(timer, &microsecond));
  g_timer_stop(timer);
  g_timer_destroy(timer);
#endif

#ifdef debug
  print_prefetch_table(Mithril_params);
#endif
}

/**
 add two associated block into prefetch table

 @param Mithril the cache struct
 @param gp1 pointer to the first block
 @param gp2 pointer to the second block
 */
static void _Mithril_add_to_prefetch_table(cache_t *cache, gpointer gp1,
                                           gpointer gp2) {
  /** currently prefetch table can only support up to 2^31 entries,
   * and this function assumes the platform is 64 bit */
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(cache->prefetcher->params);

  gint prefetch_table_index = GPOINTER_TO_INT(
      g_hash_table_lookup(Mithril_params->prefetch_hashtable, gp1));
  gint dim1 =
      (gint)floor(prefetch_table_index / (double)PREFETCH_TABLE_SHARD_SIZE);
  gint dim2 = prefetch_table_index % PREFETCH_TABLE_SHARD_SIZE *
              (Mithril_params->pf_list_size + 1);

  // insert into prefetch hashtable
  int i;
  if (prefetch_table_index) {
    // already have an entry in prefetch table, just add to that entry
    gboolean insert = TRUE;

    for (i = 1; i < Mithril_params->pf_list_size + 1; i++) {
      // if this element is already in
      // the array, then don't need add
      // again ATTENTION: the following
      // assumes a 64 bit platform
#ifdef SANITY_CHECK
      if (Mithril_params->ptable_array[dim1][dim2] != GPOINTER_TO_INT(gp1)) {
        fprintf(stderr, "ERROR prefetch table pos wrong %d %ld, dim %d %d\n",
                GPOINTER_TO_INT(gp1),
                (long)Mithril_params->ptable_array[dim1][dim2], dim1, dim2);
        exit(1);
      }
#endif
      if ((Mithril_params->ptable_array[dim1][dim2 + i]) == 0) break;
      if ((Mithril_params->ptable_array[dim1][dim2 + i]) ==
          GPOINTER_TO_INT(gp2)) {
        /* update score here, not implemented yet */
        insert = FALSE;
      }
    }

    if (insert) {
      if (i == Mithril_params->pf_list_size + 1) {
        // list full, randomly pick one for replacement
        //                i = rand()%Mithril_params->pf_list_size + 1;

        // use FIFO
        int j;
        for (j = 2; j < Mithril_params->pf_list_size + 1; j++) {
          Mithril_params->ptable_array[dim1][dim2 + j - 1] =
              Mithril_params->ptable_array[dim1][dim2 + j];
        }
        i = Mithril_params->pf_list_size;
      }
      // new add at position i
      Mithril_params->ptable_array[dim1][dim2 + i] = GPOINTER_TO_INT(gp2);
    }
  } else {
    // does not have entry, need to add a new entry
    Mithril_params->ptable_cur_row++;
    dim1 = (gint)floor(Mithril_params->ptable_cur_row /
                       (double)PREFETCH_TABLE_SHARD_SIZE);
    dim2 = Mithril_params->ptable_cur_row % PREFETCH_TABLE_SHARD_SIZE *
           (Mithril_params->pf_list_size + 1);

    /* check whether prefetch table is fully allocated, if True, we are going
     to replace the entry at ptable_cur_row by set the entry it points to as
     0, delete from prefetch_hashtable and add new entry */
    if (Mithril_params->ptable_is_full) {
      g_hash_table_remove(
          Mithril_params->prefetch_hashtable,
          GINT_TO_POINTER(Mithril_params->ptable_array[dim1][dim2]));

      memset(&(Mithril_params->ptable_array[dim1][dim2]), 0,
             sizeof(gint64) * (Mithril_params->pf_list_size + 1));
    }

    Mithril_params->ptable_array[dim1][dim2 + 1] = GPOINTER_TO_INT(gp2);
    Mithril_params->ptable_array[dim1][dim2] = GPOINTER_TO_INT(gp1);

#ifdef SANITY_CHECK
    // make sure gp1 is not in prefetch_hashtable
    if (g_hash_table_contains(Mithril_params->prefetch_hashtable, gp1)) {
      gpointer gp =
          g_hash_table_lookup(Mithril_params->prefetch_hashtable, gp1);
      printf("contains %d, value %d, %d\n", GPOINTER_TO_INT(gp1),
             GPOINTER_TO_INT(gp), prefetch_table_index);
    }
#endif

    g_hash_table_insert(Mithril_params->prefetch_hashtable, gp1,
                        GINT_TO_POINTER(Mithril_params->ptable_cur_row));

    // check current shard is full or not
    if ((Mithril_params->ptable_cur_row + 1) % PREFETCH_TABLE_SHARD_SIZE == 0) {
      /* need to allocate a new shard for prefetch table */
      if (Mithril_params->cur_metadata_size +
              PREFETCH_TABLE_SHARD_SIZE *
                  (Mithril_params->pf_list_size * 8 + 8 + 4) <
          Mithril_params->max_metadata_size) {
        Mithril_params->ptable_array[dim1 + 1] =
            g_new0(gint64, PREFETCH_TABLE_SHARD_SIZE *
                               (Mithril_params->pf_list_size + 1));
        gint required_meta_data_size =
            PREFETCH_TABLE_SHARD_SIZE *
            (Mithril_params->pf_list_size * 8 + 8 + 4);
        Mithril_params->cur_metadata_size += required_meta_data_size;

        // For the general purpose, it has been decided not to consider the
        // metadata overhead of the prefetcher

        // if(consider_metasize) {
        //   Mithril->cache_size =
        //       Mithril->cache_size -
        //       (gint)((Mithril_params->cur_metadata_size) /
        //                                    Mithril_params->block_size);
        //   cache->cache_size = Mithril->cache_size;
        //   // delay the eviction
        // }
      } else {
        Mithril_params->ptable_is_full = TRUE;
        Mithril_params->ptable_cur_row = 1;
      }
    }
  }
}

#ifdef __cplusplus
}
#endif

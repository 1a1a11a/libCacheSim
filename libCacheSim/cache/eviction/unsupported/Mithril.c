//
//  Mithril.h
//  Mithrilcache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "Mithril.h"

#include <glib.h>

// #define TRACK_BLOCK 192618l

#ifdef __cplusplus
extern "C" {
#endif

static inline void _Mithril_rec_min_support_one(cache_t *Mithril,
                                                request_t *cp);

/***************** verify function, not part of Mithril *******************/

/**
 verify the integrity of recording and mining hashtable
 */
void verify_rm_hashtable(gpointer key, gpointer value, gpointer user_data) {
  cache_t *Mithril = (cache_t *)user_data;
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);

  gint index = GPOINTER_TO_INT(value);
  if (index < 0) {
    /* in mining table */
    gint64 *row_in_mtable = GET_ROW_IN_MTABLE(Mithril_params, -index - 1);
    if (Mithril->obj_id_type == OBJ_ID_NUM) {
      /* the first entry in the mining table should be the block number */
      if (*(gint64 *)(key) != row_in_mtable[0]) {
        ERROR("hashtable mining key value not match %ld %ld\n", *(long *)(key),
              (long)(row_in_mtable[0]));
        abort();
      }
    } else {
      if (strcmp(key, (char *)(row_in_mtable[0])) != 0) {
        ERROR("hashtable mining key value not match %s %s\n", (char *)key,
              (char *)(row_in_mtable[0]));
        abort();
      }
    }
  }

  else {
    /* in recording table */
    gint64 *row_in_rtable = GET_ROW_IN_RTABLE(Mithril_params, index);
    if (Mithril->obj_id_type == OBJ_ID_NUM) {
      if (*(gint64 *)(key) != row_in_rtable[0]) {
        ERROR("hashtable recording key value not match %ld %ld\n",
              *(long *)(key), (long)(row_in_rtable[0]));
        abort();
      }
    } else {
      if (strcmp(key, (char *)(row_in_rtable[0])) != 0) {
        ERROR("hashtable recording key value not match %s %s\n", (char *)key,
              (char *)(row_in_rtable[0]));
        abort();
      }
    }
  }
}

void verify_prefetched_hashtable(gpointer key, gpointer value,
                                 gpointer user_data) {
  Mithril_params_t *Mithril_params = (Mithril_params_t *)user_data;
  GHashTable *h =
      ((struct LRU_params *)(Mithril_params->cache->cache_params))->hashtable;
  if (!g_hash_table_contains(h, key)) {
    ERROR("prefetched %ld not in cache %d\n", *(long *)key,
          GPOINTER_TO_INT(value));
    abort();
  }
}

void print_mining_table(cache_t *Mithril) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);

  rec_mining_t *rmtable = Mithril_params->rmtable;
  guint32 i;
  for (i = 0; i < rmtable->mining_table->len; i++)
    ERROR("%d: %s %ld\n", i, (char *)*(GET_ROW_IN_MTABLE(Mithril_params, i)),
          *(long *)(GET_ROW_IN_MTABLE(Mithril_params, i)));
}

void verify_mining_table(cache_t *Mithril) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  rec_mining_t *rmtable = Mithril_params->rmtable;
  guint32 i;
  for (i = 0; i < rmtable->mining_table->len; i++)
    if (((char *)*(GET_ROW_IN_MTABLE(Mithril_params, i)))[0] <= '0' ||
        ((char *)*(GET_ROW_IN_MTABLE(Mithril_params, i)))[0] > '9') {
      ERROR("check mining table error %d/%u %s %c %d\n", i,
            rmtable->mining_table->len,
            ((char *)*(GET_ROW_IN_MTABLE(Mithril_params, i))),
            ((char *)*(GET_ROW_IN_MTABLE(Mithril_params, i)))[0],
            ((char *)*(GET_ROW_IN_MTABLE(Mithril_params, i)))[0]);
      abort();
    }
}

/******************** Mithril help function ********************/

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

static inline gboolean _Mithril_remove_from_rmtable(obj_id_t obj_id_type,
                                                    rec_mining_t *rmtable,
                                                    gint64 *row) {
  if (obj_id_type == OBJ_ID_NUM) {
    return g_hash_table_remove(rmtable->hashtable, row);
  } else if (obj_id_type == OBJ_ID_STR) {
    return g_hash_table_remove(rmtable->hashtable, (char *)*row);
  } else
    fprintf(stderr,
            "does not recognize data obj_id_type in _Mithril_record_entry\n");
  return FALSE;
}

/**
 check whether last request is part of a sequential access
 */
static inline gboolean _Mithril_check_sequential(cache_t *Mithril,
                                                 request_t *cp) {
  int i;
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  if (Mithril_params->sequential_K == 0) return FALSE;

  if (Mithril->obj_id_type != OBJ_ID_NUM)
    ERROR("sequential prefetching only support data obj_id_type l\n");

  gint64 last = *(gint64 *)(cp->obj_id_ptr);
  gboolean is_sequential = TRUE;
  gpointer old_item_p = cp->obj_id_ptr;
  cp->obj_id_ptr = &last;
  gint sequential_K = Mithril_params->sequential_K;
  if (sequential_K == -1) /* when use AMP, this is -1 */
    sequential_K = 1;
  for (i = 0; i < sequential_K; i++) {
    last--;
    if (!Mithril_params->cache->check(Mithril_params->cache, cp)) {
      is_sequential = FALSE;
      break;
    }
  }
  cp->obj_id_ptr = old_item_p;
  return is_sequential;
}

void _Mithril_insert(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  Mithril_params->cache->_insert(Mithril_params->cache, cp);
}

static inline void _Mithril_record_entry(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  rec_mining_t *rmtable = Mithril_params->rmtable;

  int i;

  if (Mithril_params->rec_trigger != each_req)
    /* if it does not record at each request, check whether it is hit or miss */
    if (Mithril_params->cache->check(Mithril_params->cache, cp)) return;

  /* check it is sequtial or not */
  if (Mithril_params->sequential_type && _Mithril_check_sequential(Mithril, cp))
    return;

  if (Mithril_params->min_support == 1) {
    _Mithril_rec_min_support_one(Mithril, cp);
  }

  else {
    gint64 *row_in_rtable;
    // check the obj_id in hashtable for training
    gint index = GPOINTER_TO_INT(
        g_hash_table_lookup(rmtable->hashtable, cp->obj_id_ptr));

    if (index == 0) {
      // the node is not in the recording/mining data, should be added
      row_in_rtable = GET_CUR_ROW_IN_RTABLE(Mithril_params);
      if (cp->obj_id_type == OBJ_ID_NUM) {
#ifdef SANITY_CHECK
        if (row_in_rtable[0] != 0) {
          ERROR("recording table is not clean\n");
          abort();
        }
#endif

        row_in_rtable[0] = *(gint64 *)(cp->obj_id_ptr);
        // row_in_rtable is a pointer to the block number
        g_hash_table_insert(rmtable->hashtable, row_in_rtable,
                            GINT_TO_POINTER(rmtable->rtable_cur_row));
      } else if (cp->obj_id_type == OBJ_ID_STR) {
        gchar *str = g_strdup(cp->obj_id_ptr);
        row_in_rtable[0] = (gint64)str;
        g_hash_table_insert(rmtable->hashtable, str,
                            GINT_TO_POINTER(rmtable->rtable_cur_row));
      } else {
        ERROR("unsupport data obj_id_type in _Mithril_record_entry\n");
      }

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
        if (cp->obj_id_type == OBJ_ID_STR) {
          if (!g_hash_table_contains(rmtable->hashtable,
                                     (char *)(row_in_rtable[0])))
            ERROR(
                "remove old entry from recording table, "
                "but it is not in recording hashtable "
                "%s, pointer %ld, current ts %ld\n",
                (char *)(row_in_rtable[0]), (long)rmtable->rtable_cur_row,
                (long)Mithril_params->ts);

          g_hash_table_remove(rmtable->hashtable, (char *)(row_in_rtable[0]));
        } else if (cp->obj_id_type == OBJ_ID_NUM) {
          if (!g_hash_table_contains(rmtable->hashtable, row_in_rtable)) {
            ERROR(
                "remove old entry from recording table, "
                "but it is not in recording hashtable, "
                "block %ld, recording table pos %ld, ts %ld ",
                *(long *)row_in_rtable, (long)rmtable->rtable_cur_row,
                (long)Mithril_params->ts);

            long temp = rmtable->rtable_cur_row - 1;
            fprintf(stderr, "previous line block %ld\n",
                    *(long *)(GET_ROW_IN_RTABLE(Mithril_params, temp)));
            abort();
          }

          g_hash_table_remove(rmtable->hashtable, row_in_rtable);
        } else {
          ERROR("unsupported data obj_id_type\n");
          abort();
        }

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
        if (cp->obj_id_type == OBJ_ID_NUM) {
          if (*(gint64 *)(cp->obj_id_ptr) != row_in_mtable[0]) {
            ERROR(
                "inconsistent entry in mtable "
                "and mining hashtable current request %ld, "
                "mining table %ld\n",
                *(gint64 *)(cp->obj_id_ptr), row_in_mtable[0]);
            abort();
          }
        } else {
          if (strcmp(cp->obj_id_ptr, (char *)(row_in_mtable[0])) != 0) {
            ERROR(
                "inconsistent entry in mtable "
                "and mining hashtable current request %ld, "
                "mining table %s %s\n",
                (char *)(cp->obj_id_ptr), (char *)(row_in_mtable[0]));
            abort();
          }
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
          if (!_Mithril_remove_from_rmtable(cp->obj_id_type, rmtable,
                                            row_in_mtable))
            ERROR("removing from rmtable failed for mining table entry\n");

          /** for dataType c, now the pointer to string has been freed,
           *  so mining table entry is incorrent,
           *  but mining table entry will be deleted, so it is OK
           */

          g_array_remove_index_fast(rmtable->mining_table, -index - 1);

          /** if the removed block is not the last entry,
           *  g_array_remove_index_fast uses the last entry to fill in
           *  the old position, so we need to update its index
           **/
          if (-index - 1 != (long)rmtable->mining_table->len) {
            if (cp->obj_id_type == OBJ_ID_NUM) {
              g_hash_table_replace(rmtable->hashtable, row_in_mtable,
                                   GINT_TO_POINTER(index));
            } else if (cp->obj_id_type == OBJ_ID_STR) {
              gpointer gp = g_strdup((char *)*row_in_mtable);
              g_hash_table_insert(rmtable->hashtable, gp,
                                  GINT_TO_POINTER(index));
            }
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
        if (cp->obj_id_type == OBJ_ID_NUM) {
          if (*(gint64 *)(cp->obj_id_ptr) != row_in_rtable[0]) {
            ERROR("Hashtable recording found position not correct %ld %ld\n",
                  *(gint64 *)(cp->obj_id_ptr), row_in_rtable[0]);
            abort();
          }
        } else {
          if (strcmp(cp->obj_id_ptr, (char *)(row_in_rtable[0])) != 0) {
            ERROR("Hashtable recording found position not correct %s %s\n",
                  (char *)(cp->obj_id_ptr), (char *)(row_in_rtable[0]));
            abort();
          }
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
                    Mithril_params->ts, row_in_rtable, cur_row_in_rtable,
                    *row_in_rtable, *cur_row_in_rtable, index,
                    rmtable->rtable_cur_row - 1);
#endif
            memcpy(row_in_rtable, cur_row_in_rtable,
                   sizeof(TS_REPRESENTATION) * rmtable->rtable_row_len);
          }
          if (rmtable->rtable_cur_row >= 2)
            for (i = 0; i < rmtable->rtable_row_len; i++)
              cur_row_in_rtable[i] = 0;
          else {
            /** if current pointer points to 1,
             *  then don't move it, clear the row (that moves to mining table)
             **/
            for (i = 0; i < rmtable->rtable_row_len; i++) row_in_rtable[i] = 0;
          }

          gint64 *inserted_row_in_mtable =
              GET_ROW_IN_MTABLE(Mithril_params, rmtable->mining_table->len - 1);
          if (cp->obj_id_type == OBJ_ID_NUM) {
            /** because we don't want to have zero as index,
             *  so we add one before taking negative,
             *  in other words, the range of mining table index
             *  is -1 ~ -max_index-1, mapping to 0~max_index
             */
#ifdef SANITY_CHECK
            if (*inserted_row_in_mtable != *(gint64 *)(cp->obj_id_ptr)) {
              ERROR("current block %ld, moving mining row block %ld\n",
                    *(gint64 *)(cp->obj_id_ptr), *inserted_row_in_mtable);
              abort();
            }
#endif
            g_hash_table_replace(
                rmtable->hashtable, inserted_row_in_mtable,
                GINT_TO_POINTER(-((gint)rmtable->mining_table->len - 1 + 1)));

            if (index != rmtable->rtable_cur_row - 1 &&
                rmtable->rtable_cur_row >= 2)
              // last entry in the recording table is moved up index position
              g_hash_table_replace(rmtable->hashtable, row_in_rtable,
                                   GINT_TO_POINTER(index));
          } else if (cp->obj_id_type == OBJ_ID_STR) {
            gpointer gp1 = g_strdup((char *)(*inserted_row_in_mtable));
            /** use insert because we don't want to free the original key,
             *  instead free passed key */
            g_hash_table_insert(
                rmtable->hashtable, gp1,
                GINT_TO_POINTER(-((gint)rmtable->mining_table->len - 1 + 1)));
            if (index != rmtable->rtable_cur_row - 1 &&
                rmtable->rtable_cur_row >= 2) {
              gpointer gp2 = g_strdup((char *)(*row_in_rtable));
              g_hash_table_insert(rmtable->hashtable, gp2,
                                  GINT_TO_POINTER(index));
            }
          } else {
            ERROR("unsupported data obj_id_type\n");
            abort();
          }

          // one entry has been moved to mining table, shrinking recording table
          // size by 1
          if (rmtable->rtable_cur_row >= 2) rmtable->rtable_cur_row--;
        }
      }
    }
  }
  if (rmtable->n_avail_mining >= Mithril_params->mtable_size ||
      (Mithril_params->min_support == 1 &&
       rmtable->n_avail_mining > Mithril_params->mining_threshold / 8)) {
    _Mithril_mining(Mithril);
    rmtable->n_avail_mining = 0;
  }
}

static inline void _Mithril_rec_min_support_one(cache_t *Mithril,
                                                const request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  rec_mining_t *rmtable = Mithril_params->rmtable;

#ifdef TRACK_BLOCK
  if (*(gint64 *)(cp->obj_id_ptr) == TRACK_BLOCK) {
    int old_pos = GPOINTER_TO_INT(
        g_hash_table_lookup(rmtable->hashtable, cp->obj_id_ptr));
    printf("insert %ld, old pos %d", TRACK_BLOCK, old_pos);
    if (old_pos == 0)
      printf("\n");
    else
      printf(", block at old_pos %ld\n",
             *(gint64 *)GET_ROW_IN_MTABLE(Mithril_params, old_pos - 1));

  } else {
    gint64 b = TRACK_BLOCK;
    int old_pos = GPOINTER_TO_INT(g_hash_table_lookup(rmtable->hashtable, &b));
    if (old_pos != 0) {
      ERROR("ts %lu, checking %ld, %ld is found at pos %d\n", cp->ts,
            TRACK_BLOCK,
            *(gint64 *)GET_ROW_IN_MTABLE(Mithril_params, old_pos - 1), old_pos);
      abort();
    }
  }
#endif

  int i;
  // check the obj_id in hashtable for training
  gint index =
      GPOINTER_TO_INT(g_hash_table_lookup(rmtable->hashtable, cp->obj_id_ptr));
  if (index == 0) {
    // the node is not in the recording/mining data, should be added
    gint64 array_ele[rmtable->mtable_row_len];
    gpointer hash_key;
    if (cp->obj_id_type == OBJ_ID_STR) {
      array_ele[0] = (gint64)g_strdup(cp->obj_id_ptr);
      hash_key = (void *)(array_ele[0]);
    } else {
      array_ele[0] = *(gint64 *)(cp->obj_id_ptr);
      hash_key = GET_ROW_IN_MTABLE(Mithril_params, rmtable->mining_table->len);
    }
    for (i = 1; i < rmtable->mtable_row_len; i++) array_ele[i] = 0;
    array_ele[1] = ADD_TS(array_ele[1], Mithril_params->ts);

    g_array_append_val(rmtable->mining_table, array_ele);
    rmtable->n_avail_mining++;

    // all index is real row number + 1
    g_hash_table_insert(rmtable->hashtable, hash_key,
                        GINT_TO_POINTER(rmtable->mining_table->len));

#ifdef SANITY_CHECK
    gint64 *row_in_mtable =
        GET_ROW_IN_MTABLE(Mithril_params, rmtable->mining_table->len - 1);
    if (cp->obj_id_type == OBJ_ID_NUM) {
      if (*(gint64 *)(cp->obj_id_ptr) != row_in_mtable[0]) {
        ERROR("after inserting, hashtable mining not consistent %ld %ld\n",
              *(gint64 *)(cp->obj_id_ptr), row_in_mtable[0]);
        abort();
      }
    } else {
      if (strcmp(cp->obj_id_ptr, (char *)(row_in_mtable[0])) != 0) {
        ERROR("after inserting, hashtable mining not consistent %s %s\n",
              (char *)(cp->obj_id_ptr), (char *)(row_in_mtable[0]));
        abort();
      }
    }
#endif
  }

  else {
    /* in mining table */
    gint64 *row_in_mtable = GET_ROW_IN_MTABLE(Mithril_params, index - 1);

#ifdef SANITY_CHECK
    if (cp->obj_id_type == OBJ_ID_NUM) {
      if (*(gint64 *)(cp->obj_id_ptr) != row_in_mtable[0]) {
        ERROR("ts %lu, hashtable mining found position not correct %ld %ld\n",
              cp->ts, *(gint64 *)(cp->obj_id_ptr), row_in_mtable[0]);
        abort();
      }
    } else {
      if (strcmp(cp->obj_id_ptr, (char *)(row_in_mtable[0])) != 0) {
        ERROR("ts %lu, hashtable mining found position not correct %s %s\n",
              cp->ts, (char *)(cp->obj_id_ptr), (char *)(row_in_mtable[0]));
        abort();
      }
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
      if (!_Mithril_remove_from_rmtable(cp->obj_id_type, rmtable,
                                        row_in_mtable))
        ERROR("removing from rmtable failed for mining table entry\n");

      g_array_remove_index_fast(rmtable->mining_table, index - 1);

      // if array is moved, need to update hashtable
      if (index - 1 != (long)rmtable->mining_table->len) {
        if (cp->obj_id_type == OBJ_ID_NUM) {
          g_hash_table_replace(rmtable->hashtable, row_in_mtable,
                               GINT_TO_POINTER(index));
        } else if (cp->obj_id_type == OBJ_ID_STR) {
          gpointer gp = g_strdup((char *)*row_in_mtable);
          g_hash_table_insert(rmtable->hashtable, gp, GINT_TO_POINTER(index));
        }
      }
      rmtable->n_avail_mining--;
    }
  }
}

static inline void _Mithril_prefetch(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  gint prefetch_table_index = GPOINTER_TO_INT(
      g_hash_table_lookup(Mithril_params->prefetch_hashtable, cp->obj_id_ptr));

  gint dim1 =
      (gint)floor(prefetch_table_index / (double)PREFETCH_TABLE_SHARD_SIZE);
  gint dim2 = prefetch_table_index % PREFETCH_TABLE_SHARD_SIZE *
              (Mithril_params->pf_list_size + 1);

  if (prefetch_table_index) {
    gpointer old_cp_gp = cp->obj_id_ptr;
    int i;
    for (i = 1; i < Mithril_params->pf_list_size + 1; i++) {
      // begin from 1 because index 0 is the obj_id of originated request
      if (Mithril_params->ptable_array[dim1][dim2 + i] == 0) break;
      if (cp->obj_id_type == OBJ_ID_NUM)
        cp->obj_id_ptr = &(Mithril_params->ptable_array[dim1][dim2 + i]);
      else
        cp->obj_id_ptr = (char *)(Mithril_params->ptable_array[dim1][dim2 + i]);

      if (Mithril_params->output_statistics) Mithril_params->num_of_check += 1;

      // can't use Mithril_check here
      if (Mithril_params->cache->check(Mithril_params->cache, cp)) {
        continue;
      }
      Mithril_params->cache->_insert(Mithril_params->cache, cp);
      while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
             (long)Mithril_params->cache->size)
        // use this because we need to record stat when evicting
        _Mithril_evict(Mithril, cp);

      if (Mithril_params->output_statistics) {
        Mithril_params->num_of_prefetch_Mithril += 1;

        gpointer item_p = cp->obj_id_ptr;
        if (cp->obj_id_type == OBJ_ID_STR)
          item_p = (gpointer)g_strdup((gchar *)(cp->obj_id_ptr));

        g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril,
                            item_p, GINT_TO_POINTER(1));
      }
    }
    cp->obj_id_ptr = old_cp_gp;
  }

  // prefetch sequential
  if (Mithril_params->sequential_type == 1 &&
      _Mithril_check_sequential(Mithril, cp)) {
    gpointer old_obj_id = cp->obj_id_ptr;
    gsize next = GPOINTER_TO_SIZE(cp->obj_id_ptr) + 1;
    cp->obj_id_ptr = GSIZE_TO_POINTER(next);

    if (Mithril_params->cache->check(Mithril_params->cache, cp)) {
      Mithril_params->cache->_update(Mithril_params->cache, cp);
      cp->obj_id_ptr = old_obj_id;
      return;
    }

    // use this, not add because we need to record stat when evicting
    Mithril_params->cache->_insert(Mithril_params->cache, cp);
    while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
           Mithril_params->cache->size)
      _Mithril_evict(Mithril, cp);

    if (Mithril_params->output_statistics) {
      Mithril_params->num_of_prefetch_sequential += 1;
      g_hash_table_add(Mithril_params->prefetched_hashtable_sequential,
                       GSIZE_TO_POINTER(next));
    }
    cp->obj_id_ptr = old_obj_id;
  }
}

gboolean Mithril_check(cache_t *Mithril, request_t *cp) {
  // check element, record entry and prefetch element
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  if (Mithril_params->output_statistics) {
    if (g_hash_table_contains(Mithril_params->prefetched_hashtable_Mithril,
                              cp->obj_id_ptr)) {
      Mithril_params->hit_on_prefetch_Mithril += 1;
      g_hash_table_remove(Mithril_params->prefetched_hashtable_Mithril,
                          cp->obj_id_ptr);
    }
    if (g_hash_table_contains(Mithril_params->prefetched_hashtable_sequential,
                              cp->obj_id_ptr)) {
      Mithril_params->hit_on_prefetch_sequential += 1;
      g_hash_table_remove(Mithril_params->prefetched_hashtable_sequential,
                          cp->obj_id_ptr);
    }
  }

  gboolean result = Mithril_params->cache->check(Mithril_params->cache, cp);

  if (Mithril_params->rec_trigger != evict) _Mithril_record_entry(Mithril, cp);

  return result;
}

void _Mithril_update(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  Mithril_params->cache->_update(Mithril_params->cache, cp);
}

void _Mithril_evict(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);

  if (Mithril_params->output_statistics) {
    gpointer gp =
        Mithril_params->cache->evict_with_return(Mithril_params->cache, cp);

    gint type = GPOINTER_TO_INT(
        g_hash_table_lookup(Mithril_params->prefetched_hashtable_Mithril, gp));
    if (type != 0 && type < Mithril_params->cycle_time) {
      // give one more chance
      gpointer new_key = gp;
      if (cp->obj_id_type == OBJ_ID_STR) {
        new_key = g_strdup(gp);
      }

      g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril, new_key,
                          GINT_TO_POINTER(type + 1));
      gpointer old_cp = cp->obj_id_ptr;
      cp->obj_id_ptr = gp;
      _Mithril_insert(Mithril, cp);
      cp->obj_id_ptr = old_cp;
      _Mithril_evict(Mithril, cp);
    } else {
      gpointer old_gp = cp->obj_id_ptr;
      cp->obj_id_ptr = gp;

      if (Mithril_params->rec_trigger == evict ||
          Mithril_params->rec_trigger == miss_evict) {
        _Mithril_record_entry(Mithril, cp);
      }
      cp->obj_id_ptr = old_gp;
      g_hash_table_remove(Mithril_params->prefetched_hashtable_Mithril, gp);
      g_hash_table_remove(Mithril_params->prefetched_hashtable_sequential, gp);
    }
    g_free(gp);
  } else
    Mithril_params->cache->_evict(Mithril_params->cache, cp);
}

gboolean Mithril_add(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  gboolean retval;
  Mithril_params->ts++;

  if (strcmp(Mithril_params->cache->cache_name, "AMP")) {
    retval = Mithril_check(Mithril, cp);
    AMP_add_no_eviction(Mithril_params->cache, cp);
    _Mithril_prefetch(Mithril, cp);
    while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
           Mithril->size)
      _Mithril_evict(Mithril, cp);

  } else {
    if (Mithril_check(Mithril, cp)) {
      _Mithril_update(Mithril, cp);
      _Mithril_prefetch(Mithril, cp);
      retval = TRUE;
    } else {
      _Mithril_insert(Mithril, cp);
      _Mithril_prefetch(Mithril, cp);
      while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
             Mithril->size)
        _Mithril_evict(Mithril, cp);
      retval = FALSE;
    }
  }
  Mithril->ts += 1;
  return retval;
}

/*************** these are used for variable request size ***************/

gboolean Mithril_check_only(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  return Mithril_params->cache->check(Mithril_params->cache, cp);
}

void _Mithril_evict_only(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);

  gpointer gp;
  gp = Mithril_params->cache->evict_with_return(Mithril_params->cache, cp);

  gint type = GPOINTER_TO_INT(
      g_hash_table_lookup(Mithril_params->prefetched_hashtable_Mithril, gp));
  if (type != 0 && type < Mithril_params->cycle_time) {
    // give one more chance
    gpointer new_key = gp;
    if (cp->obj_id_type == OBJ_ID_STR) {
      new_key = g_strdup(gp);
    }
    if (Mithril_params->output_statistics)
      g_hash_table_insert(Mithril_params->prefetched_hashtable_Mithril, new_key,
                          GINT_TO_POINTER(type + 1));
    gpointer old_cp = cp->obj_id_ptr;
    cp->obj_id_ptr = gp;
    _Mithril_insert(Mithril, cp);  // insert is same
    cp->obj_id_ptr = old_cp;
    _Mithril_evict_only(Mithril, cp);
  } else {
    gpointer old_gp = cp->obj_id_ptr;
    cp->obj_id_ptr = gp;
    cp->obj_id_ptr = old_gp;
    if (Mithril_params->output_statistics) {
      g_hash_table_remove(Mithril_params->prefetched_hashtable_Mithril, gp);
      g_hash_table_remove(Mithril_params->prefetched_hashtable_sequential, gp);
    }
  }
  g_free(gp);
}

gboolean Mithril_add_only(cache_t *Mithril, request_t *cp) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  gboolean retval;

  if (strcmp(Mithril_params->cache->cache_name, "AMP")) {
    retval = Mithril_check_only(Mithril, cp);
    AMP_add_only_no_eviction(Mithril_params->cache, cp);
    while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
           Mithril->size)
      _Mithril_evict_only(Mithril, cp);
  } else {
    if (Mithril_check_only(Mithril, cp)) {
      // new
      _Mithril_update(Mithril, cp);  // update is same
      retval = TRUE;
    } else {
      _Mithril_insert(Mithril, cp);  // insert is same
      while ((long)Mithril_params->cache->get_used_size(Mithril_params->cache) >
             Mithril->size)
        _Mithril_evict_only(Mithril, cp);
      retval = FALSE;
    }
  }
  Mithril->ts += 1;
  return retval;
}

// gboolean Mithril_add_withsize(cache_t *Mithril, request_t *cp) {
//   int i;
//
//   gboolean ret_val;
//   if (Mithril->block_size != 0 && cp->disk_sector_size != 0) {
//
//     *(gint64 *)(cp->obj_id_ptr) =
//         (gint64)(*(gint64 *)(cp->obj_id_ptr) * cp->disk_sector_size /
//                  Mithril->block_size);
//   }
//
// #ifdef TRACK_BLOCK
//   Mithril_params_t *Mithril_params =
//       (Mithril_params_t *)(Mithril->cache_params);
//   rec_mining_t *rmtable = Mithril_params->rmtable;
//   static int last_found_pos = 0;
//   gint64 b = TRACK_BLOCK;
//   if (*(gint64 *)(cp->obj_id_ptr) == TRACK_BLOCK) {
//     printf("ts %lu, track block add\n", cp->ts);
//   }
//   int old_pos = GPOINTER_TO_INT(g_hash_table_lookup(rmtable->hashtable, &b));
//   if (old_pos != 0) {
//     if (old_pos > 0) {
//       gint64 *row = GET_ROW_IN_RTABLE(Mithril_params, old_pos);
//       if (*row != TRACK_BLOCK) {
//         ERROR("the row (%d) from recording table does not match track block,
//         "
//               "it is %ld\n",
//               old_pos, *row);
//         abort();
//       }
//     } else if (old_pos < 0) {
//       gint64 *row = GET_ROW_IN_MTABLE(Mithril_params, -old_pos - 1);
//       if (*row != TRACK_BLOCK) {
//         ERROR("the row (%d) from recording table does not match track block,
//         "
//               "it is %ld\n",
//               old_pos, *row);
//         abort();
//       }
//     }
//     if (last_found_pos != old_pos) {
//       ERROR("ts %lu, found track block change in hashtable, pos %d\n",
//       cp->ts,
//             old_pos);
//       last_found_pos = old_pos;
//     }
//   } else {
//     if (last_found_pos != 0) {
//       printf("ts %lu, track block %ld disappeared, might because of
//       mining\n",
//              cp->ts, TRACK_BLOCK);
//     }
//   }
// #endif
//
//   ret_val = Mithril_add(Mithril, cp);
//
//   if (Mithril->block_size != 0 && cp->disk_sector_size != 0) {
//     int n = (int)ceil((double)cp->size / Mithril->block_size);
//
//     for (i = 0; i < n - 1; i++) {
//       (*(guint64 *)(cp->obj_id_ptr))++;
//       Mithril_add_only(Mithril, cp);
//     }
//     *(gint64 *)(cp->obj_id_ptr) -= (n - 1);
//   }
//
//   Mithril->ts += 1;
//   return ret_val;
// }

void Mithril_destroy(cache_t *cache) {
  Mithril_params_t *Mithril_params = (Mithril_params_t *)(cache->cache_params);

  g_hash_table_destroy(Mithril_params->prefetch_hashtable);
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
      if (cache->obj_id_type == OBJ_ID_STR) {
        int j = 0;
        for (j = 0;
             j < PREFETCH_TABLE_SHARD_SIZE * (1 + Mithril_params->pf_list_size);
             j++)
          if ((char *)Mithril_params->ptable_array[i][j])
            g_free((char *)Mithril_params->ptable_array[i][j]);
      }
      g_free(Mithril_params->ptable_array[i]);
    } else
      break;
    i++;
  }
  g_free(Mithril_params->ptable_array);

  if (Mithril_params->output_statistics) {
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_Mithril);
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_sequential);
  }
  Mithril_params->cache->destroy(Mithril_params->cache);  // 0921
  cache_destroy(cache);
}

void Mithril_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cache, freeing these resources won't affect
   other caches copied from original cache
   */

  Mithril_params_t *Mithril_params = (Mithril_params_t *)(cache->cache_params);
  g_hash_table_destroy(Mithril_params->prefetch_hashtable);
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
      if (cache->obj_id_type == OBJ_ID_STR) {
        int j = 0;
        for (j = 0;
             j < PREFETCH_TABLE_SHARD_SIZE * (1 + Mithril_params->pf_list_size);
             j++)
          if ((char *)Mithril_params->ptable_array[i][j])
            g_free((char *)Mithril_params->ptable_array[i][j]);
      }
      g_free(Mithril_params->ptable_array[i]);
    } else
      break;
    i++;
  }
  g_free(Mithril_params->ptable_array);

  if (Mithril_params->output_statistics) {
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_Mithril);
    g_hash_table_destroy(Mithril_params->prefetched_hashtable_sequential);
  }
  Mithril_params->cache->destroy_unique(Mithril_params->cache);  // 0921
  cache_destroy_unique(cache);
}

cache_t *Mithril_init(guint64 size, obj_id_t obj_id_type, void *params) {
#ifdef SANITY_CHECK
  printf("SANITY_CHECK enabled\n");
#endif
  cache_t *cache = cache_init("Mithril", size, obj_id_type);
  cache->cache_params = g_new0(Mithril_params_t, 1);
  Mithril_params_t *Mithril_params = (Mithril_params_t *)(cache->cache_params);

  cache->cache_init = Mithril_init;
  cache->destroy = Mithril_destroy;
  cache->destroy_unique = Mithril_destroy_unique;
  cache->add = Mithril_add;
  cache->check = Mithril_check;
  cache->_insert = _Mithril_insert;
  cache->_evict = _Mithril_evict;
  cache->get_used_size = Mithril_get_size;
  cache->cache_init_params = params;
  //  cache->add_only = Mithril_add_only;
  //  cache->add_withsize = Mithril_add_withsize;

  Mithril_init_params_t *init_params = (Mithril_init_params_t *)params;

  Mithril_params->lookahead_range = init_params->lookahead_range;
  Mithril_params->max_support = init_params->max_support;
  Mithril_params->min_support = init_params->min_support;
  Mithril_params->confidence = init_params->confidence;
  Mithril_params->pf_list_size = init_params->pf_list_size;
  Mithril_params->sequential_type = init_params->sequential_type;
  Mithril_params->sequential_K = init_params->sequential_K;
  Mithril_params->cycle_time = init_params->cycle_time;
  Mithril_params->block_size = init_params->block_size;
  Mithril_params->rec_trigger = init_params->rec_trigger;
  Mithril_params->mining_threshold = init_params->mining_threshold;

  Mithril_params->mtable_size =
      (gint)(init_params->mining_threshold / Mithril_params->min_support);
  Mithril_params->rmtable = g_new0(rec_mining_t, 1);
  rec_mining_t *rmtable = Mithril_params->rmtable;
  rmtable->rtable_cur_row = 1;
  rmtable->rtable_row_len =
      (gint)ceil((double)Mithril_params->min_support / (double)4) + 1;
  rmtable->mtable_row_len =
      (gint)ceil((double)Mithril_params->max_support / (double)4) + 1;
  rmtable->mining_table =
      g_array_sized_new(FALSE, TRUE, sizeof(gint64) * rmtable->mtable_row_len,
                        Mithril_params->mtable_size);

  Mithril_params->max_metadata_size =
      (gint64)(init_params->block_size * size * init_params->max_metadata_size);
  gint max_num_of_shards_in_prefetch_table =
      (gint)(Mithril_params->max_metadata_size /
             (PREFETCH_TABLE_SHARD_SIZE * init_params->pf_list_size));
  Mithril_params->ptable_cur_row = 1;
  Mithril_params->ptable_is_full = FALSE;
  // always save to size+1 position, and enlarge table when size%shards_size ==
  // 0
  Mithril_params->ptable_array =
      g_new0(gint64 *, max_num_of_shards_in_prefetch_table);
  Mithril_params->ptable_array[0] = g_new0(
      gint64, PREFETCH_TABLE_SHARD_SIZE * (Mithril_params->pf_list_size + 1));

  /* now adjust the cache size by deducting current meta data size
   8 is the size of storage for block, 4 is the size of storage for index to
   array */
  Mithril_params->cur_metadata_size =
      (init_params->max_support * 2 + 8 + 4) * Mithril_params->mtable_size +
      max_num_of_shards_in_prefetch_table * 8 +
      PREFETCH_TABLE_SHARD_SIZE * (Mithril_params->pf_list_size * 8 + 8 + 4);

  if (Mithril_params->max_support != 1) {
    rmtable->n_rows_in_rtable =
        (gint)(size * Mithril_params->block_size * RECORDING_TABLE_MAXIMAL /
               ((gint)ceil((double)Mithril_params->min_support / (double)2) *
                    2 +
                8 + 4));
    rmtable->recording_table = g_new0(
        gint64, rmtable->n_rows_in_rtable *
                    rmtable->rtable_row_len);  // this should begins with 1
    Mithril_params->cur_metadata_size +=
        (((gint)ceil((double)init_params->min_support / (double)4 + 1) * 8 +
          4) *
         rmtable->n_rows_in_rtable);
  }

  size = size -
         (gint)(Mithril_params->cur_metadata_size / init_params->block_size);

  Mithril_params->ts = 0;

  Mithril_params->output_statistics = init_params->output_statistics;
  Mithril_params->output_statistics = 1;

  Mithril_params->hit_on_prefetch_Mithril = 0;
  Mithril_params->hit_on_prefetch_sequential = 0;
  Mithril_params->num_of_prefetch_Mithril = 0;
  Mithril_params->num_of_prefetch_sequential = 0;
  Mithril_params->num_of_check = 0;

  if (strcmp(init_params->cache_type, "LRU") == 0)
    Mithril_params->cache = LRU_init(size, obj_id_type, NULL);
  else if (strcmp(init_params->cache_type, "FIFO") == 0)
    Mithril_params->cache = FIFO_init(size, obj_id_type, NULL);
  else if (strcmp(init_params->cache_type, "LFU") == 0)  // use LFU_fast
    Mithril_params->cache = LFUFast_init(size, obj_id_type, NULL);
  else if (strcmp(init_params->cache_type, "AMP") == 0) {
    struct AMP_init_params *AMP_init_params = g_new0(struct AMP_init_params, 1);
    AMP_init_params->APT = 4;
    AMP_init_params->p_threshold = init_params->AMP_pthreshold;
    AMP_init_params->read_size = 1;
    AMP_init_params->K = init_params->sequential_K;
    Mithril_params->cache = AMP_init(size, obj_id_type, AMP_init_params);
  } else if (strcmp(init_params->cache_type, "Belady") == 0) {
    struct Belady_init_params *Belady_init_params =
        g_new(struct Belady_init_params, 1);
    Belady_init_params->reader = NULL;
    Mithril_params->cache = NULL;
    ;
  } else {
    fprintf(stderr, "can't recognize cache obj_id_type: %s\n",
            init_params->cache_type);
    Mithril_params->cache = LRU_init(size, obj_id_type, NULL);
  }

  if (obj_id_type == OBJ_ID_NUM) {
    rmtable->hashtable =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    Mithril_params->prefetch_hashtable =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    if (Mithril_params->output_statistics) {
      Mithril_params->prefetched_hashtable_Mithril =
          g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
      Mithril_params->prefetched_hashtable_sequential =
          g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    }
  } else if (obj_id_type == OBJ_ID_STR) {
    rmtable->hashtable =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    Mithril_params->prefetch_hashtable =
        g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    if (Mithril_params->output_statistics) {
      Mithril_params->prefetched_hashtable_Mithril =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
      Mithril_params->prefetched_hashtable_sequential =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    }
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  return cache;
}

gint mining_table_entry_cmp(gconstpointer a, gconstpointer b) {
  return (gint)GET_NTH_TS(a, 1) - (gint)GET_NTH_TS(b, 1);
}

void _Mithril_mining(cache_t *Mithril) {
  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);
  rec_mining_t *rmtable = Mithril_params->rmtable;
#ifdef PROFILING
  GTimer *timer = g_timer_new();
  gulong microsecond;
  g_timer_start(timer);
#endif

  _Mithril_aging(Mithril);
#ifdef PROFILING
  printf("ts: %lu, aging takes %lf seconds\n", Mithril_params->ts,
         g_timer_elapsed(timer, &microsecond));
#endif

  int i, j, k;

  /* first sort mining table, then do the mining */
  /* first remove all elements from hashtable, otherwise after sort, it will
   mess up for obj_id_type l but we can't do this for dataType c, otherwise the
   string will be freed during remove in hashtable
   */
  gint64 *item = (gint64 *)rmtable->mining_table->data;
  if (Mithril->obj_id_type == OBJ_ID_NUM) {
    for (i = 0; i < (int)rmtable->mining_table->len; i++) {
      g_hash_table_remove(rmtable->hashtable, item);
      item += rmtable->mtable_row_len;
    }
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
          Mithril_params->lookahead_range)
        break;
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
          ABS(GET_NTH_TS(item1, 1) - GET_NTH_TS(item2, 1)) == 1)
        associated_flag = TRUE;

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
        if (Mithril->obj_id_type == OBJ_ID_NUM)
          Mithril_add_to_prefetch_table(Mithril, item1, item2);
        else if (Mithril->obj_id_type == OBJ_ID_STR)
          Mithril_add_to_prefetch_table(Mithril, (char *)*item1,
                                        (char *)*item2);
      }
    }
    if (Mithril->obj_id_type == OBJ_ID_STR)
      if (!g_hash_table_remove(rmtable->hashtable, (char *)*item1)) {
        printf("ERROR remove mining table entry, but not in hash %s\n",
               (char *)*item1);
        exit(1);
      }
  }
  // delete last element
  if (Mithril->obj_id_type == OBJ_ID_STR) {
    item1 = GET_ROW_IN_MTABLE(Mithril_params, i);
    if (!g_hash_table_remove(rmtable->hashtable, (char *)*item1)) {
      printf("ERROR remove mining table entry, but not in hash %s\n",
             (char *)*item1);
      exit(1);
    }
  }

  // may be just following?
  rmtable->mining_table->len = 0;

#ifdef PROFILING
  printf("ts: %lu, clearing training data takes %lf seconds\n",
         Mithril_params->ts, g_timer_elapsed(timer, &microsecond));
  g_timer_stop(timer);
  g_timer_destroy(timer);
#endif
}

void Mithril_add_to_prefetch_table(cache_t *Mithril, gpointer gp1,
                                   gpointer gp2) {
  /** currently prefetch table can only support up to 2^31 entries, and this
   * function assumes the platform is 64 bit */

  Mithril_params_t *Mithril_params =
      (Mithril_params_t *)(Mithril->cache_params);

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
    if (Mithril->obj_id_type == OBJ_ID_NUM) {
      for (i = 1; i < Mithril_params->pf_list_size + 1; i++) {
        // if this element is already in the array, then don't need add again
        // ATTENTION: the following assumes a 64 bit platform
#ifdef SANITY_CHECK
        if (Mithril_params->ptable_array[dim1][dim2] != *(gint64 *)(gp1)) {
          fprintf(stderr, "ERROR prefetch table pos wrong %ld %ld, dim %d %d\n",
                  *(gint64 *)gp1, Mithril_params->ptable_array[dim1][dim2],
                  dim1, dim2);
          exit(1);
        }
#endif
        if ((Mithril_params->ptable_array[dim1][dim2 + i]) == 0) break;
        if ((Mithril_params->ptable_array[dim1][dim2 + i]) == *(gint64 *)(gp2))
          /* update score here, not implemented yet */
          insert = FALSE;
      }
    } else {
      for (i = 1; i < Mithril_params->pf_list_size + 1; i++) {
        // if this element is already in the cache, then don't need prefetch
        // again
        if (strcmp((char *)(Mithril_params->ptable_array[dim1][dim2]), gp1) !=
            0) {
          fprintf(stderr, "ERROR prefetch table pos wrong\n");
          exit(1);
        }
        if ((Mithril_params->ptable_array[dim1][dim2 + i]) == 0) break;
        if (strcmp((gchar *)(Mithril_params->ptable_array[dim1][dim2 + i]),
                   (gchar *)gp2) == 0)
          /* update score here, not implemented yet */
          insert = FALSE;
      }
    }

    if (insert) {
      if (i == Mithril_params->pf_list_size + 1) {
        // list full, randomly pick one for replacement
        //                i = rand()%Mithril_params->pf_list_size + 1;
        //                if (Mithril->obj_id_type == OBJ_ID_STR){
        //                    g_free((gchar*)(Mithril_params->ptable_array[dim1][dim2+i]));
        //                }

        // use FIFO
        int j;
        for (j = 2; j < Mithril_params->pf_list_size + 1; j++)
          Mithril_params->ptable_array[dim1][dim2 + j - 1] =
              Mithril_params->ptable_array[dim1][dim2 + j];
        i = Mithril_params->pf_list_size;
      }
      // new add at position i
      if (Mithril->obj_id_type == OBJ_ID_STR) {
        char *key2 = g_strdup((gchar *)gp2);
        Mithril_params->ptable_array[dim1][dim2 + i] = (gint64)key2;
      } else {
        Mithril_params->ptable_array[dim1][dim2 + i] = *(gint64 *)(gp2);
      }
    }
  } else {
    // does not have entry, need to add a new entry
    Mithril_params->ptable_cur_row++;
    dim1 = (gint)floor(Mithril_params->ptable_cur_row /
                       (double)PREFETCH_TABLE_SHARD_SIZE);
    dim2 = Mithril_params->ptable_cur_row % PREFETCH_TABLE_SHARD_SIZE *
           (Mithril_params->pf_list_size + 1);

    /* check whether prefetch table is fully allocated, if True, we are going to
     replace the entry at ptable_cur_row by set the entry it points to as 0,
     delete from prefetch_hashtable and add new entry */
    if (Mithril_params->ptable_is_full) {
      if (Mithril->obj_id_type == OBJ_ID_NUM)
        g_hash_table_remove(Mithril_params->prefetch_hashtable,
                            &(Mithril_params->ptable_array[dim1][dim2]));
      else
        g_hash_table_remove(Mithril_params->prefetch_hashtable,
                            (char *)(Mithril_params->ptable_array[dim1][dim2]));

      if (Mithril->obj_id_type == OBJ_ID_STR) {
        int i;
        for (i = 0; i < Mithril_params->pf_list_size + 1; i++) {
          g_free((gchar *)Mithril_params->ptable_array[dim1][dim2 + i]);
        }
      }

      memset(&(Mithril_params->ptable_array[dim1][dim2]), 0,
             sizeof(gint64) * (Mithril_params->pf_list_size + 1));
    }

    gpointer gp1_dup;
    if (Mithril->obj_id_type == OBJ_ID_NUM)
      gp1_dup = &(Mithril_params->ptable_array[dim1][dim2]);
    else
      gp1_dup = (gpointer)g_strdup((gchar *)(gp1));

    if (Mithril->obj_id_type == OBJ_ID_STR) {
      char *key2 = g_strdup((gchar *)gp2);
      Mithril_params->ptable_array[dim1][dim2 + 1] = (gint64)key2;
      Mithril_params->ptable_array[dim1][dim2] = (gint64)gp1_dup;
    } else {
      Mithril_params->ptable_array[dim1][dim2 + 1] = *(gint64 *)(gp2);
      Mithril_params->ptable_array[dim1][dim2] = *(gint64 *)(gp1);
    }

#ifdef SANITY_CHECK
    // make sure gp1 is not in prefetch_hashtable
    if (g_hash_table_contains(Mithril_params->prefetch_hashtable, gp1)) {
      gpointer gp =
          g_hash_table_lookup(Mithril_params->prefetch_hashtable, gp1);
      printf("ERROR datatype %c, contains %ld %s, value %d, %d\n",
             Mithril->obj_id_type, *(gint64 *)gp1, (char *)gp1,
             GPOINTER_TO_INT(gp), prefetch_table_index);
    }
#endif

    g_hash_table_insert(Mithril_params->prefetch_hashtable, gp1_dup,
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
        Mithril_params->cache->size =
            Mithril->size - (gint)((Mithril_params->cur_metadata_size) /
                                   Mithril_params->block_size);
      } else {
        Mithril_params->ptable_is_full = TRUE;
        Mithril_params->ptable_cur_row = 1;
      }
    }
  }
}

void _Mithril_aging(cache_t *Mithril) { ; }

guint64 Mithril_get_size(cache_t *cache) {
  Mithril_params_t *Mithril_params = (Mithril_params_t *)(cache->cache_params);
  return (guint64)Mithril_params->cache->get_used_size(Mithril_params->cache);
}

void prefetch_node_destroyer(gpointer data) {
  g_ptr_array_free((GPtrArray *)data, TRUE);
}

void prefetch_array_node_destroyer(gpointer data) { g_free(data); }

#ifdef __cplusplus
}
#endif

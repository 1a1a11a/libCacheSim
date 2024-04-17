//
//  Mithril.h
//  Mithrilcache
//
//  Created by Juncheng on 9/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/* 201912 due to the remove of sector_size and block_size, AMP, Mithril does not
 * work out-of-box now, new changes are needed to make them correct */

//  Modified by Zhelong on 2023/8/15

#ifndef Mithril_h
#define Mithril_h

#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../cache.h"

/** related to mining table size,
 *  mining_table_size = mining_threshold/min_support unit: entries
 **/
#define MINING_THRESHOLD 5120

/* use shards for prefetch table because
 * the size of prefetch table cannot be predicted before hand */
#define PREFETCH_TABLE_SHARD_SIZE 2000

/* recording table size, unit: percentage of cache size */
#define RECORDING_TABLE_MAXIMAL 0.02

/* check params valid */
#define check_params(params)                                                \
  (assert(params->lookahead_range > 0 && params->lookahead_range <= 100 &&  \
          params->max_support > 0 && params->min_support > 0 &&             \
          params->max_support >= params->min_support &&                     \
          params->confidence > 0 && params->confidence <= 100 &&            \
          params->pf_list_size > 0 && params->pf_list_size <= 100 &&        \
          params->max_metadata_size > 0 && params->max_metadata_size < 1 && \
          params->cycle_time > 0 && params->cycle_time <= 100 &&            \
          params->block_size > 0 && params->sequential_type >= 0 &&         \
          params->sequential_type <= 2 && params->output_statistics >= 0 && \
          params->output_statistics <= 1))

/**
 retrieve the row_num row in the recording table

 @param param Mithril_params
 @param row_num the number of row
 @return a pointer to the beginning of the row
 */
#define GET_ROW_IN_RTABLE(param, row_num) \
  ((param)->rmtable->recording_table +    \
   (row_num) * (param)->rmtable->rtable_row_len)

/**
 retrieve current row in the recording table

 @param param Mithril_params
 @return a pointer to current row
 */
#define GET_CUR_ROW_IN_RTABLE(param)   \
  ((param)->rmtable->recording_table + \
   (param)->rmtable->rtable_cur_row * (param)->rmtable->rtable_row_len)

/**
 retrieve the row_num row in the mining table

 @param param Mithril_params
 @param row_num the order of row
 @return a pointer to the beginning of the row
 */
#define GET_ROW_IN_MTABLE(param, row_num)                        \
  (((TS_REPRESENTATION *)(param)->rmtable->mining_table->data) + \
   (param)->rmtable->mtable_row_len * (row_num))

/****************************************************************************
 * macros for getting and setting time stamp in one 64-bit int
 * format of timestamps are stored in the 64-bit int
 * the first 4  bits: the number of timestamps 0~4
 * each next 15 bits: one timestamp, if timestamp larger than the max, round
 over
 * the first timestamp is stored in the   4-18 bit
 * the second timestamp is stored in the 19-33 bit
 * similarly, 34-48, 49-63 bit for the last two timestamps


 *****************************************************************************/

/**
 calculate the number of timestamps stored in one integer

 @param param the 64-bit integer
 @return the number of timestamps
 */
#define NUM_OF_TS(param) ((gint)((param) >> 60))

/**
 retrieve the Nth timestamp in the 64-bit integer

 @param param the 64-bit integer
 @param n the order, range 1-4
 @return the timestamp
 */
#define _GET_NTH_TS(param, n) \
  (((param) >> (15 * (4 - (n)))) & ((1UL << 15) - 1))

/**
 retrieve the Nth timestamp from a row in the table

 @param row a pointer to the row
 @param n the order
 @return the timestamp
 */
#define GET_NTH_TS(row, n) \
  ((gint)_GET_NTH_TS((*((gint64 *)(row) + 1 + (gint)((n) / 5))), ((n) % 4)))
// ((gint)_GET_NTH_TS((*((gint64 *)(row) + 1 + (gint)(floor((n) / 4)))), \
  //                    ((n) % 4)))

/**
 clear and set the Nth timestamp in the 64-bit integer

 @param list the 64-bit integer
 @param n the order
 @param ts timestamp to set, only lower 15 bit is used
 */
#define SET_NTH_TS(list, n, ts)                            \
  (((list) & (~(((1UL << 15) - 1) << (15 * (4 - (n)))))) | \
   (((ts) & ((1UL << 15) - 1)) << (15 * (4 - (n)))))

/**
 append a timestamp to a 64-bit integer

 @param list the 64-bit integer
 @param ts timestamp to set
 */
#define ADD_TS(list, ts)                                                       \
  (((SET_NTH_TS((list), (NUM_OF_TS((list)) + 1), (ts))) & ((1UL << 60) - 1)) | \
   ((NUM_OF_TS((list)) + 1UL) << 60))

#ifdef __cplusplus
extern "C" {
#endif

/* the representation of timestamp */
typedef gint64 TS_REPRESENTATION;

/* when recording takes place */
typedef enum _recording_loc {
  miss = 0,  // this is the default, change order will have effect
  evict,
  miss_evict,
  each_req,
} rec_trigger_e;

/* the data for Mithril initialization */
typedef struct {
  /** when we say two obj/blocks are associated,
   *  how far away they can be separated away,
   *  the more parallelism, the larger this value should be
   **/
  gint lookahead_range;

  /* max_support is the threshold for a block to become frequent block */
  gint max_support;
  /** min_support is the threshold
   *  when a block to become mature for mining,
   *  it is also the threshold for migrating a block from recording table
   *  to mining table, and it is the row size of the recording table
   **/
  gint min_support;

  /* this is now always set at 1 for precision */
  gint confidence;

  /* the number of associations can be stored in prefetching table*/
  gint pf_list_size;

  /* when to record */
  rec_trigger_e rec_trigger;

  /** size of block
   *  when all object's size is 1, should be set in eviction_params.
   *  Otherwise, should be 1 (default).
   **/
  guint64 block_size;

  /* the maximum allocated metadata size */
  gdouble max_metadata_size;

  /** when a prefetched element is going to be evicted,
   *  if it hasn't been accessed,
   *  we give it several (cycle_time-1) more chances,
   *  each time, we put it back to MRU end,
   *  the range of cycle_time is 1-3 in most cases.
   *  the idea comes from AMP.
   *  1: no cycle, no more chance, otherwise give cycle_time-1 chances
   **/
  gint cycle_time;

  /** related to mining table size,
   *  mining_table_size = mining_threshold/min_support unit: entries
   **/
  int mining_threshold;

  /** the embedded sequential prefetching method. just use in block or cache
   * line level where obj_size is same.
   * 0: no sequential_prefetching, 1: simple
   * sequential_prefetching, 2: AMP
   */
  gint sequential_type;

  /** this is the K parameter in sequential prefetching,
   *  it dictates when we think it is accessing sequentially
   *  if there have been sequential_K sequential blocks,
   *  then it is in sequential mode and activates sequential prefetching.
   **/
  gint sequential_K;

  /** this is the p parameter of AMP,
   only use it when using AMP as sequential prefetching algorithm **/
  gint AMP_pthreshold;

  /** this is the control knob for whether printing out statistics **/
  gint output_statistics;

} Mithril_init_params_t;

typedef struct {
  /** the hash table for storing block related info,
   *  currently key is the block number
   *  value is the order of row in recording table or mining table,
   *  if the value is positive, it is pointing to recording table,
   *  if it is negative, it is pointing to mining table
   **/
  GHashTable *hashtable;

  /** this is the location for storing recording table,
   *  recording table is N*(min_support/4+1) array,
   *  where 4 is the number of timestamps stored in one 64bit integer,
   *  N is number of entries in recording table, plus one is for label
   **/
  gint64 *recording_table;

  /** row len of the recording table,
   *  which is min_suuport/4 + 1 for now
   **/
  gint8 rtable_row_len;

  /** the number of rows in recording table,
   *  it decides how many blocks can be stored in the recording table,
   *  the larger recording table, the more blocks we can keep,
   *  however, if a block stays in recording table for too long,
   *  it means it is rare, as it has not accumulated enough accesses
   *  to be migrated to mining table
   *  and keeping such block is not reasonable.
   *  Therefore, giving too much space to recording table is not useful.
   **/
  gint64 n_rows_in_rtable;

  /** this is the current row number of recording table **/
  gint64 rtable_cur_row;

  /** the row len of mining table,
   *  which is max_support/4 + 1 for now
   **/
  gint8 mtable_row_len;

  /** location for mining table,
   *  use GArray for quick appending
   **/
  GArray *mining_table;

  /** this is the counter for how many obj/blocks
   *  in the mining table are ready for mining **/
  gint n_avail_mining;
} rec_mining_t;

typedef struct {
  /* see Mithril_init_params_t */
  gint lookahead_range;
  gint max_support;
  gint min_support;
  gint confidence;
  gint cycle_time;
  gint pf_list_size;
  gint mining_threshold;

  gint block_size;
  gint sequential_type;
  gint sequential_K;
  gint output_statistics;

  /* mining table size */
  gint mtable_size;

  /** when to trigger recording, miss/evict/miss_evict/each_req **/
  rec_trigger_e rec_trigger;

  /** similar to the one in init_params,
   *  but this one uses byts, not percentage
   **/
  gint64 max_metadata_size;

  /** current used metadata_size (unit: byte) **/
  gint64 cur_metadata_size;

  /* recording and mining table */
  rec_mining_t *rmtable;

  /* prefetch hashtable block -> index in ptable_array*/
  GHashTable *prefetch_hashtable;

  /* the number of current row in prefetch table */
  gint32 ptable_cur_row;

  /** a flag indicates whether prefetch table is fully allocated or not
   *  if it is full, we are replacing using a FIFO policy,
   *  and we don't allocate prefetch table shard when it
   *  reaches end of current shard
   **/
  gboolean ptable_is_full;

  /** prefetch table,
   *  this is a two dimension array for storing associations
   **/
  gint64 **ptable_array;

  /** timestamp, currently reference number **/
  guint64 ts;

  // for statistics
  GHashTable *prefetched_hashtable_Mithril;
  guint64 hit_on_prefetch_Mithril;
  guint64 num_of_prefetch_Mithril;

  GHashTable *prefetched_hashtable_sequential;
  guint64 hit_on_prefetch_sequential;
  guint64 num_of_prefetch_sequential;

  guint64 num_of_check;

  GHashTable *cache_size_map;
} Mithril_params_t;

#ifdef __cplusplus
}
#endif

#endif

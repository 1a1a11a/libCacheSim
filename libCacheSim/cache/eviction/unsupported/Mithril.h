//
//  Mithril.h
//  Mithrilcache
//
//  Created by Juncheng on 9/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/* 201912 due to the remove of sector_size and block_size, AMP, Mithril does not work out-of-box now,
 * new changes are needed to make them correct */


#ifndef Mithril_h
#define Mithril_h


#include <time.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/libCacheSim/cache.h"
#include "../dep/LRU.h"
#include "../dep/FIFO.h"
#include "LFUFast.h"
#include "Optimal.h"
#include "AMP.h"



/** related to mining table size,
 *  mining_table_size = mining_threshold/min_support unit: entries
 **/
#define MINING_THRESHOLD  5120

/* use shards for prefetch table because
 * the size of prefetch table cannot be predicted before hand */
#define PREFETCH_TABLE_SHARD_SIZE 2000


/* recording table size, unit: percentage of cache size */
#define RECORDING_TABLE_MAXIMAL 0.02


/**
 retrieve the row_num row in the recording table
 
 @param param Mithril_params
 @param row_num the number of row
 @return a pointer to the beginning of the row
 */
#define GET_ROW_IN_RTABLE(param, row_num)          \
((param)->rmtable->recording_table                      \
+ (row_num) * (param)->rmtable->rtable_row_len        \
)


/**
 retrieve current row in the recording table
 
 @param param Mithril_params
 @return a pointer to current row
 */
#define GET_CUR_ROW_IN_RTABLE(param)           \
( (param)->rmtable->recording_table                     \
+ (param)->rmtable->rtable_cur_row                   \
* (param)->rmtable->rtable_row_len                   \
)


/**
 retrieve the row_num row in the mining table
 
 @param param Mithril_params
 @param row_num the order of row
 @return a pointer to the beginning of the row
 */
#define GET_ROW_IN_MTABLE(param, row_num)                  \
( ((TS_REPRESENTATION*)(param)->rmtable->mining_table->data) \
+ (param)->rmtable->mtable_row_len * (row_num))



/****************************************************************************
 * macros for getting and setting time stamp in one 64-bit int
 * format of timestamps are stored in the 64-bit int
 * the first 4  bits: the number of timestamps 0~4
 * each next 15 bits: one timestamp, if timestamp larger than the max, round over
 * the first timestamp is stored in the   4-18 bit
 * the second timestamp is stored in the 19-33 bit
 * similarly, 34-48, 49-63 bit for the last two timestamps
 
 
 *****************************************************************************/

/**
 calculate the number of timestamps stored in one integer
 
 @param param the 64-bit integer
 @return the number of timestamps
 */
#define NUM_OF_TS(param)        ((gint)((param)>>60))


/**
 retrieve the Nth timestamp in the 64-bit integer
 
 @param param the 64-bit integer
 @param n the order, range 1-4
 @return the timestamp
 */
#define _GET_NTH_TS(param, n)    (((param)>>(15*(4-(n)))) & ((1UL<<15)-1))


/**
 retrieve the Nth timestamp from a row in the table
 
 @param row a pointer to the row
 @param n the order
 @return the timestamp
 */
#define GET_NTH_TS(row, n)       \
((gint)_GET_NTH_TS((*((gint64*)(row) + 1 + (gint)(floor((n)/4)))), ((n)%4)))


/**
 clear and set the Nth timestamp in the 64-bit integer
 
 @param list the 64-bit integer
 @param n the order
 @param ts timestamp to set, only lower 15 bit is used
 */
#define SET_NTH_TS(list, n, ts) \
(((list) & ( ~(((1UL<<15)-1)<<(15*(4-(n)))))) \
| (((ts) & ((1UL<<15)-1))<<(15*(4-(n)))))



/**
 append a timestamp to a 64-bit integer
 
 @param list the 64-bit integer
 @param ts timestamp to set
 */
#define ADD_TS(list, ts)        \
(((SET_NTH_TS((list), (NUM_OF_TS((list))+1), (ts))) \
& ((1UL<<60)-1)) | ((NUM_OF_TS((list))+1UL)<<60))


#ifdef __cplusplus
extern "C"
{
#endif


/* the representation of timestamp */
typedef gint64 TS_REPRESENTATION;

/* when recording takes place */
typedef enum _recording_loc {
  miss = 0,     // this is the default, change order will have effect
  evict,
  miss_evict,
  each_req,
} rec_trigger_e;


/* the data for Mithril initialization */
typedef struct {
  /* obj_id_type of cache, LRU, FIFO, Optimal, AMP */
  char *cache_type;

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
   *  this should be equal to block_unit_size in cache->core
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

  /** the embedded sequential prefetching method
   *  0: no sequential_prefetching,
   *  1: simple sequential_prefetching,
   *  2: AMP
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
  /* the underlying cache, like LRU, LFU, AMP */
  cache_t *cache;

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
} Mithril_params_t;


/**
 check whether an block/obj is in the cache

 @param Mithril the cache struct
 @param req the request
 @return TRUE/FASLE
 */
extern gboolean Mithril_check(cache_t *Mithril,
                                      request_t *req);


/**
 add a request to cache, if it is in the cache, update it
 if not, insert into cache, this is the upper level API
 return TRUE if request is in the cache
 return FALSE otherwise

 @param Mithril the cache struct
 @param cp the request containing the request
 @return TRUE/FALSE
 */
extern gboolean Mithril_add(cache_t *Mithril,
                                    request_t *req);


/**
 update the obj/block in the cache,
 it is used internally during a cache hit

 @param Mithril the cache struct
 @param cp the request containing the request
 */
extern void _Mithril_update(cache_t *Mithril,
                                     request_t *req);


/**
 insert the obj/block into cache
 it is used internally during a cache miss

 @param Mithril the cache struct
 @param cp the request containing the request
 */
extern void _Mithril_insert(cache_t *Mithril,
                                     request_t *req);


/**
 evict one obj/block from cache
 it is used internally when cache is full
 and a new obj/block needs to be inserted

 @param Mithril the cache struct
 @param cp the request containing the request
 */
extern void _Mithril_evict(cache_t *Mithril,
                                    request_t *req);


/**
 the free function for cache struct

 @param Mithril the cache struct
 */
extern void Mithril_destroy(cache_t *Mithril);


/**
 The difference of this and Mithril_destroy is that
 Mithril_destroy_uniq only destroy uniq struct related to current cache,
 while some shared data are not affected,
 this is going to changed in the future version as shared data will be
 moved to reader

 @param Mithril the cache struct
 */
extern void Mithril_destroy_unique(cache_t *Mithril);


/**
 the mining funciton, it is called when mining table is ready

 @param Mithril the cache struct
 */
extern void _Mithril_mining(cache_t *Mithril);

/**
 the aging function, to avoid some blocks stay too lonng,
 not used in this version.

 @param Mithril the cache struct
 */
extern void _Mithril_aging(cache_t *Mithril);


/**
 add two associated block into prefetch table

 @param Mithril the cache struct
 @param gp1 pointer to the first block
 @param gp2 pointer to the second block
 */
extern void Mithril_add_to_prefetch_table(cache_t *Mithril,
                                          gpointer gp1,
                                          gpointer gp2);


/**
 initialization of Mithril

 @param size size of cache, (unit: unit_size)
 @param data_type type of data: "l", "c"
 @param block_size unit size
 @param params Mithril_init_params_t
 @return the Mithril cache struct
 */
cache_t *Mithril_init(guint64 size,
                      obj_id_t obj_id_type,
                      void *params);


extern void prefetch_node_destroyer(gpointer data);

extern void prefetch_array_node_destroyer(gpointer data);


/**
 get current cache size usage

 @param cache cache struct
 @return current size
 */
extern guint64 Mithril_get_size(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif

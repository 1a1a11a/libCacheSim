//
//  PG.h
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//
//  Modified by Zhelong on 2/21/24.

/** since this is sequence based prefetching, we will use gint64 for block
 * number **/

#ifndef PG_h
#define PG_h

#include <glib.h>

#include "../../../dataStructure/pqueue.h"
#include "../../../traceReader/readerInternal.h"
#include "../cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/* check params valid */
#define check_params(params)                                                                                   \
  (assert(params->lookahead_range > 0 && params->lookahead_range <= 100 && params->max_metadata_size > 0 &&    \
          params->max_metadata_size < 1 && params->prefetch_threshold > 0 && params->prefetch_threshold < 1 && \
          params->block_size > 0))

#define PG_getPage(x) ((struct PG_Page*)g_hashtable_lookup())

// #define get_Nth_past_request_c(PG_params, n)
// ((char**)((PG_params)->past_requests))[(n)]
#define get_Nth_past_request_c(PG_params, n, des) strcpy((des), ((char**)((PG_params)->past_requests))[(n)])

#define get_Nth_past_request_l(PG_params, n) ((guint64*)((PG_params)->past_requests))[(n)]
#define set_Nth_past_request_c(PG_params, n, v) strcpy(((char**)((PG_params)->past_requests))[(n)], (char*)(v))
#define set_Nth_past_request_l(PG_params, n, v) ((guint64*)((PG_params)->past_requests))[(n)] = (v)

typedef struct {
  uint8_t lookahead_range;
  uint block_size;  // In the PG algorithm, the existence of block_size, like Mithril, is to correct the maximum
                    // metadata size while ignoring object size
  uint64_t cur_metadata_size;
  uint64_t max_metadata_size;  // unit byte
  double prefetch_threshold;

  gboolean stop_recording;

  GHashTable* graph;  // key -> graphNode_t
  GHashTable* prefetched;
  void* past_requests;  // past requests, using array instead of queue to avoid
                        // frequent memory allocation

  uint32_t past_request_pointer;
  gint64 num_of_prefetch;
  gint64 num_of_hit;

  GHashTable* cache_size_map;  // key -> size
} PG_params_t;

typedef struct {
  GHashTable* graph;  // key -> pq_node_t
  pqueue_t* pq;
  uint64_t total_count;
} graphNode_t;

typedef struct {
  uint8_t lookahead_range;
  uint block_size;
  double max_metadata_size;
  double prefetch_threshold;
} PG_init_params_t;

#ifdef __cplusplus
}
#endif

#endif /* PG_H */

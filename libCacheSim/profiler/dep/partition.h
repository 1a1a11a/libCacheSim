//
//  partition.h
//  libCacheSim
//
//  Created by Juncheng on 11/19/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef partition_h
#define partition_h


#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <stdint.h>
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/const.h"
#include "../../cacheAlg/dep/FIFO.h"
#include "Optimal.h"
#include "LRU_K.h"
#include "../../cacheAlg/dep/LRU.h"
#include "AMP.h"


#ifdef __cplusplus
extern "C"
{
#endif


typedef struct{
    uint8_t     n_partitions;
    uint64_t    cache_size;
    GArray**    partition_history;
    uint64_t*   current_partition;
    uint64_t    jump_over_count;            // the first m requests are not in the partition because the cache was not full at
                                            // that time
}partition_t;




partition_t* init_partition_t(
                            uint8_t n_partitions,
                            uint64_t cache_size
                            );

void free_partition_t(partition_t *partition);


partition_t* get_partition(
                           reader_t* reader,
                           struct cache* cache,
                           uint8_t n_partitions
                           );

return_res_t** profiler_partition(
                                  reader_t* reader_in,
                                  cache_t* cache_in,
                                  int num_of_threads_in,
                                  int bin_size_in
                                  );


#ifdef __cplusplus
}
#endif


#endif /* partition_h */

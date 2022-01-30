#pragma once 


#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"



bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs); 

bucket_t *select_segs_rand(cache_t *cache, segment_t **segs); 

bucket_t *select_segs_learned(cache_t *cache, segment_t **segs);

bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs);

void rank_segs(cache_t *cache); 


#pragma once 


#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

bucket_t *select_segs_to_evict(cache_t *cache, segment_t *segs[]); 

void L2Cache_merge_segs(cache_t *cache, bucket_t *bucket, segment_t *segs[]); 



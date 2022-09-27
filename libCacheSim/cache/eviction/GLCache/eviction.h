#pragma once 


#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "GLCacheInternal.h"

bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs); 

void GLCache_merge_segs(cache_t *cache, bucket_t *bucket, segment_t **segs); 

int evict_one_seg(cache_t *cache, segment_t *seg); 

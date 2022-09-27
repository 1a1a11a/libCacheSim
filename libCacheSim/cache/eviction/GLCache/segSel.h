#pragma once

#include "../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "GLCacheInternal.h"

bucket_t *select_segs_fifo(cache_t *cache, segment_t **segs);

bucket_t *select_segs_weighted_fifo(cache_t *cache, segment_t **segs);

bucket_t *select_segs_rand(cache_t *cache, segment_t **segs);

bucket_t *select_segs_learned(cache_t *cache, segment_t **segs);

bucket_t *select_segs_to_evict(cache_t *cache, segment_t **segs);

void rank_segs(cache_t *cache);

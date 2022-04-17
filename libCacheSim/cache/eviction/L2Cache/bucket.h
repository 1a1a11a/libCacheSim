#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

/* append a segment to the end of bucket */
void append_seg_to_bucket(L2Cache_params_t *params, bucket_t *bucket, segment_t *segment);

void remove_seg_from_bucket(L2Cache_params_t *params, bucket_t *bucket, segment_t *segment);

int find_bucket_idx(L2Cache_params_t *params, request_t *req);

#ifdef USE_LHD
void update_hit_prob_cdf(bucket_t *bkt);
#endif

void print_bucket(cache_t *cache); 

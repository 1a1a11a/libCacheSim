#pragma once 

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"


/* append a segment to the end of bucket */
static inline void append_seg_to_bucket(L2Cache_params_t *params, bucket_t *bucket,
                                        segment_t *segment) {
  /* because the last segment may not be full, so we link before it */
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    DEBUG_ASSERT(bucket->n_seg == 0);
    bucket->first_seg = segment;
    bucket->next_seg_to_evict = segment;
    if (&params->training_bucket != bucket)
      params->n_used_buckets += 1;
  } else {
    bucket->last_seg->next_seg = segment;
  }

  segment->prev_seg = bucket->last_seg;
  segment->next_seg = NULL;
  bucket->last_seg = segment;

  bucket->n_seg += 1;
  if (&params->training_bucket == bucket) {
    params->n_training_segs += 1;
  } else {
    params->n_segs += 1;
  }
}

static inline void remove_seg_from_bucket(L2Cache_params_t *params, bucket_t *bucket,
                                          segment_t *segment) {
  if (bucket->first_seg == segment) {
    bucket->first_seg = segment->next_seg;
  }
  if (bucket->last_seg == segment) {
    bucket->last_seg = segment->prev_seg;
  }

  if (segment->prev_seg != NULL) {
    segment->prev_seg->next_seg = segment->next_seg;
  }

  if (segment->next_seg != NULL) {
    segment->next_seg->prev_seg = segment->prev_seg;
  }

  params->n_segs -= 1;
  bucket->n_seg -= 1;

  if (bucket->n_seg == 0) {
    params->n_used_buckets -= 1;
  }
}


static inline int find_bucket_idx(L2Cache_params_t *params, request_t *req) {
  const double log_base = log(2);

  if (params->bucket_type == NO_BUCKET) {
    return 0;
  } else if (params->bucket_type == SIZE_BUCKET) {
    if (params->size_bucket_base == 1)
        return sizeof(unsigned int)*8 - 1 - __builtin_clz(req->obj_size);
    else
      return MAX(0, (int) (log((double) req->obj_size / params->size_bucket_base) / log_base));
//    return MAX(0, (int) (log(req->obj_size / 10.0) / log_base));
//    return MAX(0, (int) (log(req->obj_size / 120.0) / log_base));
  } else if (params->bucket_type == CUSTOMER_BUCKET) {
    return req->tenant_id % 8;
  } else if (params->bucket_type == BUCKET_ID_BUCKET) {
    return req->ns % 8;
  } else if (params->bucket_type == CONTENT_TYPE_BUCKET) {
    return req->content_type;
  } else {
    printf("unknown bucket type %d\n", params->bucket_type);
    abort();
  }
}


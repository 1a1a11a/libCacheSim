
#include "GLCacheInternal.h"

/* append a segment to the end of bucket */
void append_seg_to_bucket(GLCache_params_t *params, bucket_t *bucket,
                          segment_t *segment) {
  /* because the last segment may not be full, so we link before it */
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    DEBUG_ASSERT(bucket->n_in_use_segs == 0);
    bucket->first_seg = segment;
    bucket->next_seg_to_evict = segment;
    if (&params->train_bucket != bucket) params->n_used_buckets += 1;
  } else {
    bucket->last_seg->next_seg = segment;
  }

  segment->prev_seg = bucket->last_seg;
  segment->next_seg = NULL;
  bucket->last_seg = segment;

  bucket->n_in_use_segs += 1;
  if (&params->train_bucket == bucket) {
    params->n_training_segs += 1;
  } else {
    params->n_in_use_segs += 1;
  }
}

void remove_seg_from_bucket(GLCache_params_t *params, bucket_t *bucket,
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

  params->n_in_use_segs -= 1;
  bucket->n_in_use_segs -= 1;

  if (bucket->n_in_use_segs == 0) {
    params->n_used_buckets -= 1;
  }
}

void print_bucket(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;

  printf("bucket has segs: ");
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    if (params->buckets[i].n_in_use_segs != 0) {
      printf("%d (%d), ", i, params->buckets[i].n_in_use_segs);
    }
  }
  printf("\n");
}

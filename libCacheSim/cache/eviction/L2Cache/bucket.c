
#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"

/* append a segment to the end of bucket */
void append_seg_to_bucket(L2Cache_params_t *params, bucket_t *bucket, segment_t *segment) {
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

void remove_seg_from_bucket(L2Cache_params_t *params, bucket_t *bucket, segment_t *segment) {
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

int find_bucket_idx(L2Cache_params_t *params, request_t *req) {
  const double log_base = log(2);

  if (params->bucket_type == NO_BUCKET) {
    return 0;
  } else if (params->bucket_type == SIZE_BUCKET) {
    return sizeof(unsigned int) * 8 - 1 - __builtin_clz(req->obj_size);
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

void update_hit_prob_cdf(bucket_t *bkt) {

  for (int cl = 0; cl < HIT_PROB_CLASSES; cl ++){
    hitProb_t *hb = bkt->hit_prob;
    int64_t n_hit_sum = hb->n_hit[cl][HIT_PROB_MAX_AGE - 1];
    int64_t n_event_sum = n_hit_sum + hb->n_evict[cl][HIT_PROB_MAX_AGE - 1];
    int64_t lifetime_uncond = n_event_sum;

    for (int i = HIT_PROB_MAX_AGE - 2; i >= 0; i--) {

      n_hit_sum += hb->n_hit[cl][i];
      n_event_sum += hb->n_hit[cl][i] + hb->n_evict[cl][i];
      lifetime_uncond += n_event_sum;

      hb->n_hit[cl][i] *= LHD_EWMA;
      hb->n_evict[cl][i] *= LHD_EWMA;

      if (n_event_sum > 0) {
        hb->hit_density[cl][i] = (double) n_hit_sum * 1e10 / lifetime_uncond;
      } else {
        hb->hit_density[cl][i] = 1e-8;
      }
    }

    static int last_overflow = 0;
    if (bkt->hit_prob->n_overflow > 0) {
      if (bkt->hit_prob->n_overflow > last_overflow) {
        DEBUG("bucket %d overflow %ld\n", bkt->bucket_id, (long) bkt->hit_prob->n_overflow);
        last_overflow = bkt->hit_prob->n_overflow;
      }
      bkt->hit_prob->n_overflow = 0;
    }
  }
}

void print_bucket(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  printf("bucket has segs: ");
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    if (params->buckets[i].n_in_use_segs != 0) {
      printf("%d (%d), ", i, params->buckets[i].n_in_use_segs);
    }
  }
  printf("\n");
}

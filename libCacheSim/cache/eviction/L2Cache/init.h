

#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "const.h"



void init_seg_sel(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  params->seg_sel.score_array_size = params->n_merge * params->segment_size;
  params->seg_sel.score_array = my_malloc_n(double, params->seg_sel.score_array_size);

  params->seg_sel.last_rank_time = -INT32_MAX;
  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;

  params->seg_sel.segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
  memset(params->seg_sel.segs_to_evict, 0, sizeof(segment_t *) * params->n_merge);
}

void init_learner(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  L2Cache_init_params_t *init_params = cache->init_params;

  learner_t *l = &params->learner;

  l->n_feature = N_FEATURE_TIME_WINDOW * 3 + 12;
  l->pred = NULL;
  l->training_x = NULL;
  l->train_matrix_size_row = 0;
  l->valid_matrix_size_row = 0;
  l->inference_data = NULL;
  l->inf_matrix_size_row = 0;

  l->n_train = 0;
  l->n_inference = 0;

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
  l->n_segs_to_start_training = 1024*8;
  l->n_bytes_start_collect_train = cache->cache_size / 1;

  if (l->sample_every_n_seg_for_training <= 0)
    l->sample_every_n_seg_for_training = 1;
#else
//  l->n_max_training_segs = 1024 * 16;
  l->n_max_training_segs = 1024 * 8;
//  l->next_snapshot_evicted_bytes = cache->cache_size * 1;
  l->n_snapshot = 0;
  l->last_snapshot_rtime = 0;
  l->snapshot_intvl = init_params->snapshot_intvl;
  if (l->snapshot_intvl <= 0)
    l->snapshot_intvl = 3600 * 2;
#endif

  l->re_train_intvl = init_params->re_train_intvl;
  if (l->re_train_intvl <= 0)
    l->re_train_intvl = 86400;

  l->n_evicted_bytes = 0;
  l->last_train_rtime = 0;

}

static void init_buckets(cache_t *cache, int age_shift) {
  L2Cache_params_t *params = cache->eviction_params;

  if (age_shift <= 0)
    age_shift = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    params->buckets[i].bucket_idx = i;
    params->buckets[i].hit_prob = my_malloc(hitProb_t);
    for (int j = 0; j < HIT_PROB_MAX_AGE; j++) {
      /* initialize to a small number, when the hit density is not available
       * before eviction, we use size to make eviction decision */
      params->buckets[i].hit_prob->hit_density[j] = 1e-8;
      params->buckets[i].hit_prob->age_shift = age_shift;
    }
  }
}

static void init_cache_state(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  params->cache_state.cold_miss_ratio = -1;
  params->cache_state.write_ratio = -1;
  params->cache_state.req_rate = -1;
  params->cache_state.write_rate = -1;
}
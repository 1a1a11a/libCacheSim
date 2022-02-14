

#pragma once

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "const.h"

void init_global_params() {
#ifdef COMPARE_TRAINING_Y
  ofile_cmp_y = fopen(COMPARE_TRAINING_Y_FILE, "w");
  fprintf(ofile_cmp_y, "# segment utility calculated online and offline, format: bucket, "
                       "segment, online, offline\n");
#endif
}

void deinit_global_params() {
  if (ofile_cmp_y) {
    fclose(ofile_cmp_y);
  }
}

void check_init_params(L2Cache_init_params_t *init_params) {
  assert(init_params->segment_size > 1 && init_params->segment_size <= 100000000);
  assert(init_params->n_merge > 1 && init_params->n_merge <= 100);
  assert(init_params->rank_intvl > 0 && init_params->rank_intvl < 1);
  assert(init_params->segment_size / init_params->n_merge > 1);
}

void init_seg_sel(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  params->seg_sel.last_rank_time = -INT32_MAX;
  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;
}

void init_obj_sel(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  params->obj_sel.score_array_size = params->n_merge * params->segment_size;
  params->obj_sel.score_array = my_malloc_n(double, params->obj_sel.score_array_size);

  params->obj_sel.segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
  memset(params->obj_sel.segs_to_evict, 0, sizeof(segment_t *) * params->n_merge);
}

void init_learner(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  L2Cache_init_params_t *init_params = cache->init_params;

  learner_t *l = &params->learner;

  l->n_feature = N_FEATURE_TIME_WINDOW * 3 + 12;
  l->pred = NULL;
  l->train_x = NULL;
  l->train_matrix_n_row = 0;
  l->valid_matrix_n_row = 0;
  l->inference_x = NULL;
  l->inf_matrix_n_row = 0;

  l->n_train = -1;
  l->n_inference = 0;

  l->train_matrix_n_row = 1024 * 8;
  l->train_x = my_malloc_n(feature_t, l->train_matrix_n_row * l->n_feature);
  l->train_y = my_malloc_n(pred_t, l->train_matrix_n_row);
  l->valid_matrix_n_row = l->train_matrix_n_row / 10;
  l->valid_x = my_malloc_n(feature_t, l->valid_matrix_n_row * l->n_feature);
  l->valid_y = my_malloc_n(train_y_t, l->valid_matrix_n_row);

  l->retrain_intvl = init_params->retrain_intvl;
  if (l->retrain_intvl <= 0) l->retrain_intvl = 86400;

  l->last_train_rtime = 0;
}

static void init_buckets(cache_t *cache, int age_shift) {
  L2Cache_params_t *params = cache->eviction_params;

  if (age_shift <= 0) age_shift = 0;

  for (int i = 0; i < MAX_N_BUCKET; i++) {
    memset(&params->buckets[i], 0, sizeof(bucket_t));
    params->buckets[i].bucket_id = i;
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
  params->cache_state.miss_ratio = -1;
  params->cache_state.req_rate = -1;
  params->cache_state.write_rate = -1;
}
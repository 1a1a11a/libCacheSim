
#include "GLCacheInternal.h"
#include "const.h"

void init_global_params() {
#ifdef COMPARE_TRAINING_Y
  ofile_cmp_y = fopen("compare_y.txt", "w");
  fprintf(ofile_cmp_y,
          "# segment utility calculated online and offline, format: bucket, "
          "segment, online, offline\n");
#endif
}

void deinit_global_params() {
  if (ofile_cmp_y != NULL) {
    fclose(ofile_cmp_y);
  }
}

void check_params(GLCache_params_t *params) {
  assert(params->segment_size > 1 && params->segment_size <= 100000000);
  assert(params->n_merge > 1 && params->n_merge <= 100);
  assert(params->rank_intvl > 0 && params->rank_intvl < 1);
  assert(params->segment_size / params->n_merge > 1);
}

void init_seg_sel(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;

  params->seg_sel.ranked_segs = NULL;
  params->seg_sel.ranked_seg_size = -1;
  params->seg_sel.ranked_seg_pos = 0;
}

void init_obj_sel(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;

  params->obj_sel.array_size = params->n_merge * params->segment_size;
  params->obj_sel.score_array = my_malloc_n(double, params->obj_sel.array_size);
#ifdef RANDOMIZE_MERGE
  params->obj_sel.score_array_offset =
      my_malloc_n(double, params->obj_sel.array_size);
#endif
  params->obj_sel.dd_pair_array =
      my_malloc_n(dd_pair_t, params->obj_sel.array_size);

  params->obj_sel.segs_to_evict = my_malloc_n(segment_t *, params->n_merge);
  memset(params->obj_sel.segs_to_evict, 0,
         sizeof(segment_t *) * params->n_merge);
}

void init_learner(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;

  learner_t *l = &params->learner;

  l->n_feature = N_FEATURE_TIME_WINDOW * 3 + N_FEATURE_NORMAL;
  l->pred = NULL;
  l->train_x = NULL;
  l->train_matrix_n_row = 0;
  l->valid_matrix_n_row = 0;
  l->inference_x = NULL;
  l->inf_matrix_n_row = 0;

  l->n_train = -1;
  l->n_inference = 0;

  l->train_matrix_n_row = N_MAX_TRAINING_DATA;
  l->train_x = my_malloc_n(feature_t, l->train_matrix_n_row * l->n_feature);
  memset(l->train_x, 0,
         sizeof(feature_t) * l->train_matrix_n_row * l->n_feature);
  l->train_y = my_malloc_n(pred_t, l->train_matrix_n_row);
  memset(l->train_y, 0, sizeof(pred_t) * l->train_matrix_n_row);
  l->train_y_oracle = my_malloc_n(pred_t, l->train_matrix_n_row);
  memset(l->train_y_oracle, 0, sizeof(pred_t) * l->train_matrix_n_row);
  l->valid_matrix_n_row = l->train_matrix_n_row / 10;
  l->valid_x = my_malloc_n(feature_t, l->valid_matrix_n_row * l->n_feature);
  memset(l->valid_x, 0,
         sizeof(feature_t) * l->valid_matrix_n_row * l->n_feature);
  l->valid_y = my_malloc_n(train_y_t, l->valid_matrix_n_row);
  memset(l->valid_y, 0, sizeof(train_y_t) * l->valid_matrix_n_row);
  // l->retrain_intvl = retrain_intvl;
  l->last_train_rtime = 0;
}

static void init_buckets(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;
  for (int i = 0; i < MAX_N_BUCKET; i++) {
    memset(&params->buckets[i], 0, sizeof(bucket_t));
    params->buckets[i].bucket_id = i;
  }
}

void init_cache_state(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;
  params->cache_state.miss_ratio = 0.5;
  params->cache_state.req_rate = 1;
  params->cache_state.write_rate = 1;
}
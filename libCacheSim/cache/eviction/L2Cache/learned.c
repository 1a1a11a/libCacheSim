

#include "learned.h"
#include "learnInternal.h"
#include "L2CacheInternal.h"
#include "const.h"
#include "utils.h"

static void clean_training_segs(cache_t *cache, int n_clean) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t *seg = params->training_bucket.first_seg;
  segment_t *next_seg;
  int n_cleaned = 0;

  while (n_cleaned < n_clean) {
    DEBUG_ASSERT(seg != NULL);
    next_seg = seg->next_seg;
    clean_one_seg(cache, seg);
    seg = next_seg;
    n_cleaned += 1;
  }

  if (n_clean == params->n_training_segs) {
    DEBUG_ASSERT(seg == NULL);
    params->training_bucket.last_seg = NULL;
  }

  params->n_training_segs -= n_cleaned;
  params->training_bucket.n_seg -= n_cleaned;
  params->training_bucket.first_seg = seg;
  seg->prev_seg == NULL;
}

static inline void create_data_holder(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &params->learner;

  int32_t n_max_training_samples = params->n_training_segs;
  if (learner->train_matrix_row_len < n_max_training_samples) {
    n_max_training_samples *= 2;
    if (learner->train_matrix_row_len > 0) {
      DEBUG_ASSERT(learner->train_x != NULL);
      my_free(sizeof(feature_t) * learner->train_matrix_row_len * learner->n_feature,
              learner->train_x);
      my_free(sizeof(train_y_t) * learner->train_matrix_row_len, learner->train_y);
    }
    learner->train_x = my_malloc_n(feature_t, n_max_training_samples * learner->n_feature);
    learner->train_y = my_malloc_n(pred_t, n_max_training_samples);
    learner->train_matrix_row_len = n_max_training_samples;
  }

  if (learner->valid_matrix_row_len < N_MAX_VALIDATION) {
    if (learner->valid_matrix_row_len > 0) {
      DEBUG_ASSERT(learner->valid_x != NULL);
      my_free(sizeof(feature_t) * learner->valid_matrix_row_len * learner->n_feature,
              learner->valid_x);
      my_free(sizeof(train_y_t) * learner->valid_matrix_row_len, learner->valid_y);
      //      my_free(sizeof(pred_t) * learner->valid_matrix_row_len, learner->valid_pred_y);
    }
    learner->valid_x = my_malloc_n(feature_t, N_MAX_VALIDATION * learner->n_feature);
    learner->valid_y = my_malloc_n(train_y_t, N_MAX_VALIDATION);
    //    learner->valid_pred_y = my_malloc_n(pred_t, N_MAX_VALIDATION);
    learner->valid_matrix_row_len = N_MAX_VALIDATION;
  }
}

// // #if TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
// #if defined(TRAINING_DATA_SOURCE) && TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
// void create_data_holder2(cache_t *cache) {
//   L2Cache_params_t *params = cache->eviction_params;
//   learner_t *learner = &params->learner;

//   if (learner->train_matrix_row_len == 0) {
//     learner->train_x =
//         my_malloc_n(feature_t, learner->n_max_training_segs * learner->n_feature);
//     learner->train_y = my_malloc_n(pred_t, learner->n_max_training_segs);
//     learner->train_matrix_row_len = learner->n_max_training_segs;

//     learner->valid_matrix_row_len = learner->n_max_training_segs / 10;
//     learner->valid_x =
//         my_malloc_n(feature_t, learner->valid_matrix_row_len * learner->n_feature);
//     learner->valid_y = my_malloc_n(train_y_t, learner->valid_matrix_row_len);
//   }
// }
// #endif


#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
static inline void copy_to_train_matrix(cache_t *cache, segment_t *seg) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  if (l->n_training_samples >= l->n_max_training_segs - 1) return;

  unsigned int row_idx = l->n_training_samples++;

  seg->in_training_data = true;
  seg->training_data_row_idx = row_idx;
  seg->become_train_seg_rtime = params->curr_rtime;
  seg->become_train_seg_vtime = params->curr_vtime;

  prepare_one_row(cache, seg, true, &l->train_x[row_idx * l->n_feature],
                  &l->train_y[row_idx]);
}
#endif

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
void snapshot_segs_to_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;
  segment_t *curr_seg = NULL;

  l->n_snapshot += 1;
  l->last_snapshot_rtime = params->curr_rtime;
  int sample_n_segs = l->n_max_training_segs / (l->re_train_intvl / l->snapshot_intvl);

  int sample_ratio = MAX(params->n_segs / sample_n_segs, 1);

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg - 2; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      if (rand() % sample_ratio == 0) {
        copy_to_train_matrix(cache, curr_seg);
      }

      curr_seg = curr_seg->next_seg;
    }
  }
  int n_snapshot_per_train = l->re_train_intvl / 3600;
  printf("time %ld (%ld day %ld hour %ld sec) "
         "snapshot %d: %d/%d sample\n",
         (long) params->curr_rtime, (long) params->curr_rtime / 86400,
         (long) params->curr_rtime % 86400 / 3600, (long) params->curr_rtime % 3600,
         l->n_snapshot, l->n_training_samples, l->n_max_training_segs);
}
#endif



static void train_eval(const int iter, const double *pred, const float *true_y, int n_elem) {
  int pred_selected = find_argmin_double(pred, n_elem);
  int true_selected = find_argmin_float(true_y, n_elem);
  double val_in_pred_selected_by_true = pred[true_selected];
  double val_in_true_selected_by_pred = true_y[pred_selected];

  int pred_selected_rank_in_true = 0, true_selected_rank_in_pred = 0;

  for (int i = 0; i < n_elem; i++) {
    if (pred[i] < val_in_pred_selected_by_true) {
      true_selected_rank_in_pred += 1;
    }
    if (true_y[i] < val_in_true_selected_by_pred) {
      pred_selected_rank_in_true += 1;
    }
  }

  printf("iter %d: %d/%d pred:true pos %d/%d val %lf %f\n", iter, pred_selected_rank_in_true,
         true_selected_rank_in_pred, pred_selected, true_selected, pred[pred_selected],
         true_y[true_selected]);
  //    abort();
}


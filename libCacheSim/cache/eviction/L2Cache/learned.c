

#include "learned.h"
#include "L2CacheInternal.h"
#include "const.h"
#include "learnInternal.h"
#include "utils.h"

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
/** @brief copy a segment to training data matrix
 *
 * @param cache
 * @param seg
 */
static inline void copy_seg_to_train_matrix(cache_t *cache, segment_t *seg) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  if (l->n_train_samples >= l->train_matrix_n_row) return;

  unsigned int row_idx = l->n_train_samples++;

  seg->in_training_data = true;
  seg->training_data_row_idx = row_idx;
  seg->become_train_seg_rtime = params->curr_rtime;
  seg->become_train_seg_vtime = params->curr_vtime;
  seg->utility = 0;

  prepare_one_row(cache, seg, true, &l->train_x[row_idx * l->n_feature], &l->train_y[row_idx]);
}

/** @brief sample some segments and copy their features to training data matrix 
 *
 * @param cache
 * @param seg
 */
void snapshot_segs_to_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;
  segment_t *curr_seg = NULL;

  double sample_ratio = MAX((double) params->n_segs / (double) l->train_matrix_n_row, 1.0);

  double credit = 0;// when credit reaches sample ratio, we sample a segment
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_segs - 1; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      credit += 1;
      if (credit >= sample_ratio) {
        credit -= sample_ratio;
        copy_seg_to_train_matrix(cache, curr_seg);
      } else {
        curr_seg->in_training_data = false;
      }

      curr_seg = curr_seg->next_seg;
    }
  }
  INFO("%.2lf hour snapshot %d/%d train sample\n", 
       (double) params->curr_rtime / 3600.0,
       l->n_train_samples, l->train_matrix_n_row);
}
#endif

// validation on training data
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

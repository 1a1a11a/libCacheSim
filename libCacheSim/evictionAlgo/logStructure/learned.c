

#include "learned.h"

#include <LightGBM/c_api.h>

#define safe_call(call)                                                                       \
  {                                                                                           \
    int err = (call);                                                                         \
    if (err != 0) {                                                                           \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, LastErrorMsg()); \
      exit(1);                                                                                \
    }                                                                                         \
  }

static inline int32_t idx_to_age(cache_t *cache, int idx) {
  LLSC_params_t *params = cache->cache_params;
  return params->learner.feature_history_time_window * idx;
}

static void clean_training_segs(cache_t *cache, int n_clean) {
  LLSC_params_t *params = cache->cache_params;
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

static double cal_seg_evict_penalty_learned(cache_t *cache, segment_t *seg) {
  ;
}

static inline void create_data_holder(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  int32_t n_max_training_samples = params->n_training_segs * N_FEATURE_TIME_WINDOW;
  if (learner->training_n_row < n_max_training_samples) {
    n_max_training_samples *= 2;
    if (learner->training_n_row > 0) {
      DEBUG_ASSERT(learner->training_x != NULL);
      my_free(sizeof(double) * learner->training_n_row * learner->n_feature, learner->training_x);
      my_free(sizeof(float) * learner->training_n_row, learner->training_y);
    }
    learner->training_x = my_malloc_n(double, n_max_training_samples * learner->n_feature);
    learner->training_y = my_malloc_n(float, n_max_training_samples);
    learner->training_n_row = n_max_training_samples;
  }

  if (learner->validation_n_row < N_MAX_VALIDATION) {
    if (learner->validation_n_row > 0) {
      DEBUG_ASSERT(learner->valid_x != NULL);
      my_free(sizeof(double) * learner->validation_n_row * learner->n_feature, learner->valid_x);
      my_free(sizeof(float) * learner->validation_n_row, learner->valid_y);
      my_free(sizeof(double) * learner->validation_n_row, learner->valid_pred_y);
    }
    learner->valid_x = my_malloc_n(double, N_MAX_VALIDATION * learner->n_feature);
    learner->valid_y = my_malloc_n(float, N_MAX_VALIDATION);
    learner->valid_pred_y = my_malloc_n(double, N_MAX_VALIDATION);
    learner->validation_n_row = N_MAX_VALIDATION;
  }
}

static inline bool prepare_one_row(cache_t *cache, segment_t *curr_seg, int j, int seg_age, double *x, float *y) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  /* suppose we are at the end of j window, the first j features are available */
  x[0] = 0;
  /* CHANGE ######################### */
  x[1] = (double) curr_seg->bucket_idx;
  /* CHANGE ######################### */
  x[2] = (double) ((curr_seg->create_rtime / 3600) % 24);
  if (seg_age >= 0) {
    x[3] = (double) seg_age;
  } else {
    x[3] = (double) idx_to_age(cache, j + 1);
  }
  x[4] = (double) curr_seg->req_rate;
  x[5] = (double) curr_seg->write_rate;
  x[6] = (double) curr_seg->total_byte / curr_seg->n_total_obj;

  for (int k = 0; k <= j; k++) {
    //        int32_t window_size = idx_to_window_size(j + k);
    int32_t window_size = learner->feature_history_time_window;
    x[7 + k * 5 + 0] =
        (double) curr_seg->feature.n_hit[k] / window_size;
    x[7 + k * 5 + 1] =
        (double) curr_seg->feature.n_active_item_per_window[k] / window_size;
    x[7 + k * 5 + 2] =
        (double) curr_seg->feature.n_active_item_accu[k] / window_size;
    /* CHANGE ######################### */
    x[7 + k * 5 + 3] =
        (double) curr_seg->feature.n_active_byte_per_window[k] / window_size;
    x[7 + k * 5 + 4] =
        (double) curr_seg->feature.n_active_byte_accu[k] / window_size;
  }

//  printf("%lf %lf %lf %lf %lf %lf %lf - %lf %lf %lf %lf %lf\n",
//         x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11]);


  for (unsigned int k = j + 1; k < N_FEATURE_TIME_WINDOW; k++) {
    x[7 + k * 5 + 0] = 0;
    x[7 + k * 5 + 1] = 0;
    x[7 + k * 5 + 2] = 0;
    x[7 + k * 5 + 3] = 0;
    x[7 + k * 5 + 4] = 0;
  }

  if (y == NULL) {
    return true;
  }

  /* calculate y */
  double penalty, n_obj, mean_rd, mean_sz;
  int n_retained_obj;
  n_retained_obj = curr_seg->n_total_obj / params->n_merge;
  //      n_retained_obj = 0;

  //      /* maybe the retained obj should consider the difficulty of finding them */
  //      penalty = 0;
  //      n_obj = (double) curr_seg->feature.n_active_item_per_window[j + 1];
  //      /* CHANGE ######################### */
  //      mean_rd = idx_to_age(cache, j+1) - idx_to_age(cache, j);
  //      mean_sz = (double) curr_seg->feature.n_active_byte_per_window[j + 1] / n_obj;
  //
  //      if (n_obj >= n_retained_obj) {
  //        /* CHANGE ######################### */
  //        penalty += (n_obj - n_retained_obj) * 10000 / (mean_rd * mean_sz);
  //        n_retained_obj = 0;
  //      } else {
  //        n_retained_obj -= n_obj;
  //      }
  //
  //      for (int k = j + 2; k < N_FEATURE_TIME_WINDOW; k++) {
  //        if (curr_seg->feature.n_active_item_accu[k] == 0) {
  //          DEBUG_ASSERT(curr_seg->feature.n_active_byte_accu[k] == 0);
  //          continue;
  //        }
  //        n_obj = (double) curr_seg->feature.n_active_item_accu[k];
  //        /* CHANGE ######################### */
  ////        mean_rd = seg_hist_bin_ts * k;
  //        mean_rd = idx_to_age(cache, k) - idx_to_age(cache, j);
  //        mean_sz = (double) curr_seg->feature.n_active_byte_accu[k]  / curr_seg->feature.n_active_item_accu[k];
  //        if (n_obj >= n_retained_obj) {
  //          penalty += (n_obj - n_retained_obj) * 10000 / (mean_rd * mean_sz);
  //          n_retained_obj = 0;
  //        } else {
  //          n_retained_obj -= n_obj;
  //        }
  //      }
  //
  //      DEBUG_ASSERT(penalty < INT32_MAX);
  //      if (penalty < 0.0001) {
  //        n_zeros += 1;
  //        continue;
  //      }
  //      /* CHANGE ######################### */
  //      y[n_train_samples] = (float) penalty;

  /* how many active bytes within one full_cache_write_time */
  //      n_active_byte = curr_seg->feature.n_active_byte_per_window[j + 1];
  //      int n_window = (int) rtime_write_full_cache / seg_hist_bin_ts + 1;
  //      n_window = n_window > SEG_FEATURE_HISTORY_LEN ? SEG_FEATURE_HISTORY_LEN : n_window;
  //
  //
  //      for (int k = j + 2; k < n_window; k++) {
  //          n_active_byte += curr_seg->feature.n_active_byte_accu[k];
  //      }
  //
  //      y[n_train_samples] = (float) n_active_byte / heap.seg_size;
  //      printf("%lf\n", y[n_train_samples]);

  penalty = cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, n_retained_obj,
                            -1, curr_seg->eviction_vtime);

  DEBUG_ASSERT(penalty < INT32_MAX);
  /* CHANGE ######################### */
  *y = (float) penalty;

  if (penalty < 0.0001) {
    return false;
  }
  return true;
}

static void prepare_training_data(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  create_data_holder(cache);

  double *train_x = learner->training_x;
  float *train_y = learner->training_y;
  double *valid_x = learner->valid_x;
  float *valid_y = learner->valid_y;

  int64_t n_zeros = 0;

  int i;
  int n_train_samples = 0, n_validation_samples = 0;
  segment_t *curr_seg = params->training_bucket.first_seg;

  bool valid_sample;
  /* CHANGE ######################### */
  for (i = 0; i < params->n_training_segs; i++) {
    DEBUG_ASSERT(curr_seg != NULL);
    //    for (int j = 0; j < N_FEATURE_TIME_WINDOW / 2; j++) {
//    int curr_idx = seg_history_idx(curr_seg, params->curr_time, learner->feature_history_time_window);
//    for (int j = curr_idx; j < curr_idx + 1; j++) {
      /* if we have no or little future information,
       * we cannot have a good estimate of y */
      //      if (curr_seg->feature.n_active_item_per_window[j + 1] == 0 ||
      //          curr_seg->feature.n_active_item_per_window[j + 2] == 0 ||
      //          curr_seg->feature.n_active_item_per_window[j + 3] == 0 ||
      //          curr_seg->feature.n_active_item_per_window[j + 4] == 0) {
      //        break;
      //      }

      if (n_validation_samples < N_MAX_VALIDATION && rand() % params->n_training_segs <= 10) {

        valid_sample = prepare_one_row(
            cache, curr_seg, N_FEATURE_TIME_WINDOW-1, -1,
            &valid_x[learner->n_feature * n_validation_samples],
            &valid_y[n_validation_samples]);

        if (valid_sample) {
          n_validation_samples += 1;
        } else {
          n_validation_samples += 1;
          n_zeros += 1;
        }
      } else {
        valid_sample = prepare_one_row(
            cache, curr_seg, N_FEATURE_TIME_WINDOW-1, -1,
            &train_x[learner->n_feature * n_train_samples],
            &train_y[n_train_samples]);

        if (valid_sample) {
          n_train_samples += 1;
        } else {
          n_train_samples += 1;
          n_zeros += 1;
        }
      }
//    }
    curr_seg = curr_seg->next_seg;
  }
  learner->n_training_samples = n_train_samples;
  printf("curr time %ld (vtime %ld) training %d segs %d samples, %d validation samples, "
         "%ld total segs \n",
         (long) params->curr_time, (long) params->curr_vtime,
         (int) params->n_training_segs, n_train_samples, (int) n_validation_samples, (long) params->n_segs);

  clean_training_segs(cache, params->n_training_segs);
  printf("%ld zero %ld training segs\n", n_zeros, params->n_training_segs);
  params->learner.n_byte_written = 0;
}

static DatasetHandle transform_training_data_lgbm(cache_t *cache) {
  DatasetHandle tdata;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  char *dataset_params = "categorical_feature=1,2";
  safe_call(LGBM_DatasetCreateFromMat(learner->training_x,
                                      C_API_DTYPE_FLOAT64,
                                      learner->n_training_samples,
                                      learner->n_feature,
                                      1, dataset_params, NULL, &tdata));

  safe_call(LGBM_DatasetSetField(tdata,
                                 "label",
                                 learner->training_y,
                                 learner->n_training_samples,
                                 C_API_DTYPE_FLOAT32));

  return tdata;
}

static int find_argmin_float(const float *y, int n_elem) {
  float min_val = (float) INT64_MAX;
  int min_val_pos = -1;
  for (int i = 0; i < n_elem; i++) {
    if (y[i] < min_val) {
      min_val = y[i];
      min_val_pos = i;
    }
  }
  DEBUG_ASSERT(min_val_pos != -1);
  return min_val_pos;
}

static int find_argmin_double(const double *y, int n_elem) {
  double min_val = (double) INT64_MAX;
  int min_val_pos = -1;
  for (int i = 0; i < n_elem; i++) {
    if (y[i] < min_val) {
      min_val = y[i];
      min_val_pos = i;
    }
  }
  DEBUG_ASSERT(min_val_pos != -1);
  return min_val_pos;
}

static void train_eval(const int iter,
                       const double *pred,
                       const float *true_y,
                       int n_elem) {
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

  printf("iter %d: %d/%d pred:true pos %d/%d val %lf %f\n", iter,
         pred_selected_rank_in_true,
         true_selected_rank_in_pred,
         pred_selected, true_selected,
         pred[pred_selected], true_y[true_selected]);
  //    abort();
}

void train(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_training_data(cache);
  DatasetHandle tdata = transform_training_data_lgbm(cache);

  char *training_params = "num_leaves=31 "
//                          "bossting=dart "
                          "objective=regression "
//                          "reg_sqrt=true "
                          "linear_tree=false "
                          "force_row_wise=true "
                          "verbosity=0 "
                          "early_stopping_round=5 "

                          "feature_fraction=0.8 "
                          "bagging_freq=5 "
                          "bagging_fraction=0.8 "
                          "learning_rate=0.1 "

                          "num_threads=1";

  //    "metric=l2,l1 "
  //    "num_iterations=120 "
  //    training_params = "";

  int64_t out_len;

  safe_call(LGBM_BoosterFree(learner->booster));
  safe_call(LGBM_BoosterCreate(tdata, training_params, &learner->booster));

  int i;
  for (i = 0; i < N_TRAIN_ITER; i++) {
    int is_finished;
    safe_call(LGBM_BoosterUpdateOneIter(learner->booster, &is_finished));
    if (is_finished) {
      break;
    }
  }

  static time_t last_eval_time = 0;
  //  if (time(NULL) - last_eval_time > 20) {
//  safe_call(LGBM_BoosterPredictForMat(learner->booster,
//                                      learner->valid_x,
//                                      C_API_DTYPE_FLOAT64,
//                                      learner->n_valid_samples,
//                                      learner->n_feature,
//                                      1,
//                                      C_API_PREDICT_NORMAL,
//                                      0,
//                                      -1,
//                                      "linear_tree=false",
//                                      &out_len,
//                                      learner->valid_pred_y));
//
//  DEBUG_ASSERT(out_len == learner->n_valid_samples);
//  train_eval(i, learner->valid_pred_y,
//             learner->valid_y,
//             learner->n_valid_samples);
//  last_eval_time = time(NULL);
  //  }

  learner->n_train += 1;
}

void prepare_inference_data(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  if (learner->inference_n_row < params->n_segs) {
    if (learner->inference_n_row != 0) {
      DEBUG_ASSERT(learner->inference_data != NULL);
      my_free(sizeof(double) * learner->inference_n_row * learner->n_feature, learner->inference_data);
      my_free(sizeof(double) * learner->inference_n_row, learner->pred);
    }
    int n_row = params->n_segs * 2;
    learner->inference_data = my_malloc_n(double, n_row * learner->n_feature);
    learner->pred = my_malloc_n(double, n_row);
    learner->inference_n_row = n_row;
  }

  double *x = learner->inference_data;

  segment_t *curr_seg = params->buckets[0].first_seg;
  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      prepare_one_row(cache, curr_seg, N_FEATURE_TIME_WINDOW-1,
                      (int) (params->curr_time - curr_seg->create_rtime),
                      &x[learner->n_feature * n_seg], NULL);
      curr_seg = curr_seg->next_seg;
      n_seg++;
    }
  }
  DEBUG_ASSERT(params->n_segs == n_seg);
}

void inference(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_inference_data(cache);

  //  for (int i = 0; i < params->n_segs; i++)
  //    learner->pred[i] = INT32_MAX;

  int64_t out_len = 0;
  safe_call(LGBM_BoosterPredictForMat(learner->booster,
                                      learner->inference_data, C_API_DTYPE_FLOAT64,
                                      params->n_segs, learner->n_feature, 1,
                                      C_API_PREDICT_NORMAL, 0, -1,
                                      "num_threads=1", &out_len, learner->pred));

  DEBUG_ASSERT(out_len == params->n_segs);

  segment_t *curr_seg = params->buckets[0].first_seg;
  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      curr_seg->penalty = learner->pred[n_seg++];
      curr_seg = curr_seg->next_seg;
    }
  }

  learner->n_inference += 1;
}

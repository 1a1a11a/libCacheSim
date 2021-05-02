

#include "learned.h"

//#include <LightGBM/c_api.h>
#ifdef USE_XGBOOST
#include <xgboost/c_api.h>
#endif

//#define safe_call(call)                                                                       \
//  {                                                                                           \
//    int err = (call);                                                                         \
//    if (err != 0) {                                                                           \
//      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, LastErrorMsg()); \
//      exit(1);                                                                                \
//    }                                                                                         \
//  }

#define safe_call(call) {  \
  int err = (call); \
  if (err != 0) { \
    fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, XGBGetLastError());  \
    exit(1); \
  } \
}

//static inline int32_t idx_to_age(cache_t *cache, int idx) {
//  LLSC_params_t *params = cache->cache_params;
//  return params->learner.feature_history_time_window * idx;
//}

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

static inline void create_data_holder(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  int32_t n_max_training_samples = params->n_training_segs * N_FEATURE_TIME_WINDOW;
  if (learner->training_n_row < n_max_training_samples) {
    n_max_training_samples *= 2;
    if (learner->training_n_row > 0) {
      DEBUG_ASSERT(learner->training_x != NULL);
      my_free(sizeof(feature_t) * learner->training_n_row * learner->n_feature, learner->training_x);
      my_free(sizeof(train_y_t) * learner->training_n_row, learner->training_y);
    }
    learner->training_x = my_malloc_n(feature_t, n_max_training_samples * learner->n_feature);
    learner->training_y = my_malloc_n(pred_t, n_max_training_samples);
    learner->training_n_row = n_max_training_samples;
  }

  if (learner->validation_n_row < N_MAX_VALIDATION) {
    if (learner->validation_n_row > 0) {
      DEBUG_ASSERT(learner->valid_x != NULL);
      my_free(sizeof(feature_t) * learner->validation_n_row * learner->n_feature, learner->valid_x);
      my_free(sizeof(train_y_t) * learner->validation_n_row, learner->valid_y);
      my_free(sizeof(pred_t) * learner->validation_n_row, learner->valid_pred_y);
    }
    learner->valid_x = my_malloc_n(feature_t, N_MAX_VALIDATION * learner->n_feature);
    learner->valid_y = my_malloc_n(train_y_t, N_MAX_VALIDATION);
    learner->valid_pred_y = my_malloc_n(pred_t, N_MAX_VALIDATION);
    learner->validation_n_row = N_MAX_VALIDATION;
  }
}

static inline bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool training_data, feature_t *x, train_y_t *y) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  /* suppose we are at the end of j window, the first j features are available */
  x[0] = (feature_t) curr_seg->bucket_idx;
  /* CHANGE ######################### */
  x[1] = (feature_t) ((curr_seg->create_rtime / 3600) % 24);
  x[2] = (feature_t) ((curr_seg->create_rtime / 60) % 60);
  /* CHANGE ######################### */
  if (training_data)
    x[3] = (feature_t) curr_seg->eviction_rtime - curr_seg->create_rtime;
  else
    x[3] = (feature_t) params->curr_rtime - curr_seg->create_rtime;
  x[4] = (feature_t) curr_seg->req_rate;
  x[5] = (feature_t) curr_seg->write_rate;
  x[6] = (feature_t) curr_seg->total_byte / curr_seg->n_total_obj;
  x[7] = curr_seg->write_ratio;
  x[8] = curr_seg->cold_miss_ratio;
  x[9] = curr_seg->n_total_hit;
  x[10] = curr_seg->n_total_active;
  x[11] = curr_seg->n_merge;

  for (int k = 0; k < N_FEATURE_TIME_WINDOW; k++) {
    x[12 + k * 6 + 0] = (feature_t) curr_seg->feature.n_hit_per_min[k];
//    x[12 + k * 6 + 1] = (feature_t) curr_seg->feature.n_active_item_per_min[k];
    x[12 + k * 6 + 1] = (feature_t) curr_seg->feature.n_hit_per_ten_min[k];
//    x[12 + k * 6 + 3] = (feature_t) curr_seg->feature.n_active_item_per_ten_min[k];
    x[12 + k * 6 + 2] = (feature_t) curr_seg->feature.n_hit_per_hour[k];
//    x[12 + k * 6 + 5] = (feature_t) curr_seg->feature.n_active_item_per_hour[k];
  }

//  printf("%f %f %f %f %f %f %f %f %f %f %f - %f %f %f %f %f %f\n",
//         x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10],
//         x[11], x[12], x[13], x[14], x[15], x[16]);

  if (y == NULL) {
    return true;
  }

  /* calculate y */
  double penalty = curr_seg->penalty;

  int n_retained_obj = 0;
#if TRAINING_CONSIDER_RETAIN == 1
  n_retained_obj = params->n_retain_from_seg;
#endif


#if TRAINING_TRUTH == TRAINING_TRUTH_ORACLE
  penalty = cal_seg_penalty(cache,
                            OBJ_SCORE_ORACLE,
                            curr_seg, n_retained_obj,
                            curr_seg->eviction_rtime,
                            curr_seg->eviction_vtime);
#endif

  DEBUG_ASSERT(penalty < INT32_MAX);
  *y = (train_y_t) penalty;

  if (penalty < 0.000001) {
    return false;
  }
  return true;
}

static void prepare_training_data(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &params->learner;

  create_data_holder(cache);

  feature_t *train_x = learner->training_x;
  train_y_t *train_y = learner->training_y;
  feature_t *valid_x = learner->valid_x;
  train_y_t *valid_y = learner->valid_y;

  int n_zeros = 0;
  int n_zero_use = 0;

  int i;
  int n_train_samples = 0, n_validation_samples = 0;
  segment_t *curr_seg = params->training_bucket.first_seg;

  bool is_sample_valid;
  for (i = 0; i < params->n_training_segs / 2; i++) {
    DEBUG_ASSERT(curr_seg != NULL);


//      if (n_validation_samples < N_MAX_VALIDATION && rand() % (params->n_training_segs/100) <= 0) {
      if (0) {
        is_sample_valid = prepare_one_row(
            cache, curr_seg, true,
            &valid_x[learner->n_feature * n_validation_samples],
            &valid_y[n_validation_samples]);

        if (is_sample_valid) {
          n_validation_samples += 1;
        } else {
          n_validation_samples += 1;
          n_zeros += 1;
        }
      } else {
        is_sample_valid = prepare_one_row(
            cache, curr_seg, true,
            &train_x[learner->n_feature * n_train_samples],
            &train_y[n_train_samples]);

        if (is_sample_valid) {
          n_train_samples += 1;
        } else {
          /* do not want too much zero */
          if (n_zeros != 0 && rand() % n_zeros <= 1) {
            n_train_samples += 1;
            n_zero_use += 1;
          }
          n_zeros += 1;
        }
      }
//    }
    curr_seg = curr_seg->next_seg;
  }
  learner->n_training_samples = n_train_samples;
  INFO("cache size %lu, curr time %ld (vtime %ld) training %d segs %d samples, "
       "%d validation samples, sample every %d segs, rank intvl %d, %d (%d) zeros, "
         "%ld total segs \n", (unsigned long) cache->cache_size,
         (long) params->curr_rtime, (long) params->curr_vtime,
         (int) params->n_training_segs, n_train_samples, (int) n_validation_samples,
       params->learner.sample_every_n_seg_for_training, params->rank_intvl,
       n_zeros, n_zero_use, (long) params->n_segs);

  clean_training_segs(cache, params->n_training_segs/2);
  params->learner.n_byte_written = 0;

#ifdef USE_XGBOOST
  if (params->learner.n_train > 0) {
    safe_call(XGDMatrixFree(learner->train_dm));
    safe_call(XGDMatrixFree(learner->valid_dm));
  }

  safe_call(XGDMatrixCreateFromMat(learner->training_x,
                                   learner->n_training_samples,
                                   learner->n_feature, -1, &learner->train_dm));
//  safe_call(XGDMatrixCreateFromMat_omp(learner->training_x,
//                                   learner->n_training_samples,
//                                   learner->n_feature, -1, &learner->train_dm, 1));

  safe_call(XGDMatrixCreateFromMat(learner->valid_x,
                                   learner->n_valid_samples,
                                   learner->n_feature, -1, &learner->valid_dm));

  safe_call(XGDMatrixSetFloatInfo(learner->train_dm, "label",
                                  learner->training_y,
                                  learner->n_training_samples));

  safe_call(XGDMatrixSetFloatInfo(learner->valid_dm, "label",
                                  learner->valid_y,
                                  learner->n_valid_samples));
#elif defined(USE_GBM)
  char *dataset_params = "categorical_feature=0,1,2";
  safe_call(LGBM_DatasetCreateFromMat(learner->training_x,
                                      C_API_DTYPE_FLOAT64,
                                      learner->n_training_samples,
                                      learner->n_feature,
                                      1, dataset_params, NULL, &learner->train_dm));

  safe_call(LGBM_DatasetSetField(learner->train_dm,
                                 "label",
                                 learner->training_y,
                                 learner->n_training_samples,
                                 C_API_DTYPE_FLOAT32));
#endif
}


void prepare_inference_data(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  if (learner->inference_n_row < params->n_segs) {
    if (learner->inference_n_row != 0) {
      DEBUG_ASSERT(learner->inference_data != NULL);
      my_free(sizeof(feature_t) * learner->inference_n_row * learner->n_feature, learner->inference_data);
      my_free(sizeof(pred_t) * learner->inference_n_row, learner->pred);
    }
    int n_row = params->n_segs * 2;
    learner->inference_data = my_malloc_n(feature_t, n_row * learner->n_feature);
    learner->pred = my_malloc_n(pred_t, n_row);
    learner->inference_n_row = n_row;
  }

  feature_t *x = learner->inference_data;

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      prepare_one_row(cache, curr_seg, false,
                      &x[learner->n_feature * n_seg], NULL);
//      printf("%f %f %f %f %f %f %f %f\n", x[learner->n_feature * n_seg],
//             x[learner->n_feature * n_seg+1],
//             x[learner->n_feature * n_seg+2],
//             x[learner->n_feature * n_seg+3],
//             x[learner->n_feature * n_seg+4],
//             x[learner->n_feature * n_seg+5],
//             x[learner->n_feature * n_seg+6],
//             x[learner->n_feature * n_seg+7]
//             );
      curr_seg = curr_seg->next_seg;
      n_seg++;
    }
  }
  DEBUG_ASSERT(params->n_segs == n_seg);

#ifdef USE_XGBOOST
  if (params->learner.n_inference > 0) {
    safe_call(XGDMatrixFree(learner->inf_dm));
  }
  safe_call(XGDMatrixCreateFromMat(learner->inference_data,
                                   params->n_segs,
                                   learner->n_feature, -1,
                                   &learner->inf_dm));
#elif defined(USE_GBM)
  ;
#endif
}

#ifdef USE_XGBOOST
static void train_xgboost(cache_t *cache) {
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_training_data(cache);

//  if (learner->n_train > 0)
//    safe_call(XGBoosterFree(learner->booster));
  DMatrixHandle eval_dmats[1] = {learner->train_dm};
  safe_call(XGBoosterCreate(eval_dmats, 1, &learner->booster));

  safe_call(XGBoosterSetParam(learner->booster, "booster", "gbtree"));
//  safe_call(XGBoosterSetParam(learner->booster, "booster", "gblinear"));
  safe_call(XGBoosterSetParam(learner->booster, "verbosity", "1"));
  safe_call(XGBoosterSetParam(learner->booster, "nthread", "1"));
//  safe_call(XGBoosterSetParam(learner->booster, "eta", "0.1"));
//  safe_call(XGBoosterSetParam(learner->booster, "gamma", "1"));
  safe_call(XGBoosterSetParam(learner->booster, "objective", "reg:squarederror"));


  for (int i = 0; i < N_TRAIN_ITER; ++i) {
    // Update the model performance for each iteration
    safe_call(XGBoosterUpdateOneIter(learner->booster, i, learner->train_dm));
  }


#ifdef DUMP_MODEL
  if (learner->n_train == DUMP_MODEL - 1)
  safe_call(XGBoosterSaveModel(learner->booster, "model.bin"));
#endif
}
#endif


#ifdef USE_XGBOOST
void inference_xgboost(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_inference_data(cache);


  /* pred result stored in xgboost lib */
  const float *pred;

  bst_ulong out_len = 0;
  safe_call(XGBoosterPredict(learner->booster,
                             learner->inf_dm, 0, 0, 0, &out_len,
                              &pred));

  DEBUG_ASSERT(out_len == params->n_segs);

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      curr_seg->penalty = learner->pred[n_seg++];
      curr_seg = curr_seg->next_seg;
    }
  }
}
#endif
















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

#ifdef USE_GBM
void train_lgbm(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;
  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_training_data(cache);

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
  safe_call(LGBM_BoosterCreate(learner->train_dm, training_params, &learner->booster));

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

  LGBM_DatasetFree(tdata)l;

}
#endif


#ifdef USE_GBM
void inference_lgbm(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  learner_t *learner = &((LLSC_params_t *) cache->cache_params)->learner;

  prepare_inference_data(cache);


  int64_t out_len = 0;
  safe_call(LGBM_BoosterPredictForMat(learner->booster,
                                      learner->inference_data, C_API_DTYPE_FLOAT64,
                                      params->n_segs, learner->n_feature, 1,
                                      C_API_PREDICT_NORMAL, 0, -1,
                                      "num_threads=1", &out_len, learner->pred));

  DEBUG_ASSERT(out_len == params->n_segs);

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      curr_seg->penalty = learner->pred[n_seg++];
      curr_seg = curr_seg->next_seg;
    }
  }

}
#endif



void train(cache_t *cache) {
  train_xgboost(cache);

  LLSC_params_t *params = (LLSC_params_t *) cache->cache_params;
  params->learner.n_train += 1;
  params->learner.last_train_rtime = params->curr_rtime;
  params->learner.last_train_vtime = params->curr_vtime;
}

void inference(cache_t *cache) {
  inference_xgboost(cache);

  LLSC_params_t *params = (LLSC_params_t *) cache->cache_params;
  params->learner.n_inference += 1;
}




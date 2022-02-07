

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "bucket.h"
#include "learnInternal.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"
#include "utils.h"

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
void transform_seg_to_training(cache_t *cache, bucket_t *bucket, segment_t *segment) {
  static int n = 0, n_zero = 0;
  L2Cache_params_t *params = cache->eviction_params;
  segment->is_training_seg = true;
  /* used to calculate the eviction penalty */
  segment->become_train_seg_vtime = params->curr_vtime;
  /* used to calculate age at eviction for training */
  segment->become_train_seg_rtime = params->curr_rtime;
  segment->penalty = 0;

  /* remove from the bucket */
  remove_seg_from_bucket(params, bucket, segment);

  append_seg_to_bucket(params, &params->train_bucket, segment);
}

static void clean_training_segs(cache_t *cache, int n_clean) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t *seg = params->train_bucket.first_seg;
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
    params->train_bucket.last_seg = NULL;
  }

  params->n_training_segs -= n_cleaned;
  params->train_bucket.n_segs -= n_cleaned;
  params->train_bucket.first_seg = seg;
  seg->prev_seg == NULL;
}

static void create_data_holder(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &params->learner;

  int32_t n_max_training_samples = params->n_training_segs;
  if (learner->train_matrix_n_row < n_max_training_samples) {
    n_max_training_samples *= 2;
    if (learner->train_matrix_n_row > 0) {
      DEBUG_ASSERT(learner->train_x != NULL);
      my_free(sizeof(feature_t) * learner->train_matrix_n_row * learner->n_feature,
              learner->train_x);
      my_free(sizeof(train_y_t) * learner->train_matrix_n_row, learner->train_y);
    }
    learner->train_x = my_malloc_n(feature_t, n_max_training_samples * learner->n_feature);
    learner->train_y = my_malloc_n(pred_t, n_max_training_samples);
    learner->train_matrix_n_row = n_max_training_samples;
  }

  if (learner->valid_matrix_n_row < N_MAX_VALIDATION) {
    if (learner->valid_matrix_n_row > 0) {
      DEBUG_ASSERT(learner->valid_x != NULL);
      my_free(sizeof(feature_t) * learner->valid_matrix_n_row * learner->n_feature,
              learner->valid_x);
      my_free(sizeof(train_y_t) * learner->valid_matrix_n_row, learner->valid_y);
      //      my_free(sizeof(pred_t) * learner->valid_matrix_n_row, learner->valid_pred_y);
    }
    learner->valid_x = my_malloc_n(feature_t, N_MAX_VALIDATION * learner->n_feature);
    learner->valid_y = my_malloc_n(train_y_t, N_MAX_VALIDATION);
    //    learner->valid_pred_y = my_malloc_n(pred_t, N_MAX_VALIDATION);
    learner->valid_matrix_n_row = N_MAX_VALIDATION;
  }
}
#endif

/* used when the training y is calculated online */
void update_train_y(L2Cache_params_t *params, cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->L2Cache.segment;

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
  /* if training data is generated from eviction, then update_train_y should 
     * be called only for evicted objects */
  DEBUG_ASSERT(cache_obj->L2Cache.in_cache == 0);
  DEBUG_ASSERT(seg->is_training_seg == true);
#elif TRAINING_DATA_SOURCE == TRAINING_X_FROM_CACHE
  /* if training data is generated from snapshot, then update_train_y should 
     * be called only for objects that are in the snapshot */
  if (!seg->in_training_data) {
    return;
  }
#else
#error "invalid TRAINING_DATA_SOURCE"
#endif

#if TRAINING_CONSIDER_RETAIN == 1
  if (seg->n_skipped_penalty++ >= params->n_retain_per_seg)
#endif
  {
    double age = (double) params->curr_vtime - seg->become_train_seg_vtime;
    //TODO: should age be in real time?
    if (params->obj_score_type == OBJ_SCORE_FREQ
        || params->obj_score_type == OBJ_SCORE_FREQ_AGE) {
      seg->train_utility += 1.0e8 / age;
    } else {
      seg->train_utility += 1.0e8 / age / cache_obj->obj_size;
    }
    params->learner.train_y[seg->training_data_row_idx] = seg->train_utility;
  }
}

int cmp_train_y(const void *p1, const void *p2) {
  const train_y_t *d1 = p1;
  const train_y_t *d2 = p2;

  if (*d1 > *d2) {
    return 1;
  } else
    return -1;
}

void prepare_training_data_per_package(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

#ifdef USE_XGBOOST
  if (learner->n_train > 0) {
    safe_call(XGDMatrixFree(learner->train_dm));
    safe_call(XGDMatrixFree(learner->valid_dm));
  }

  safe_call(XGDMatrixCreateFromMat(learner->train_x, learner->n_train_samples,
                                   learner->n_feature, -2, &learner->train_dm));

  safe_call(XGDMatrixCreateFromMat(learner->valid_x, learner->n_valid_samples,
                                   learner->n_feature, -2, &learner->valid_dm));

  safe_call(XGDMatrixSetFloatInfo(learner->train_dm, "label", learner->train_y,
                                  learner->n_train_samples));

  safe_call(XGDMatrixSetFloatInfo(learner->valid_dm, "label", learner->valid_y,
                                  learner->n_valid_samples));

#if OBJECTIVE == LTR
  safe_call(XGDMatrixSetUIntInfo(learner->train_dm, "group", &learner->n_train_samples, 1));
  safe_call(XGDMatrixSetUIntInfo(learner->valid_dm, "group", &learner->n_valid_samples, 1));
#endif

#elif defined(USE_GBM)
  char *dataset_params = "categorical_feature=0,1,2";
  safe_call(LGBM_DatasetCreateFromMat(learner->train_x, C_API_DTYPE_FLOAT64,
                                      learner->n_train_samples, learner->n_feature, 1,
                                      dataset_params, NULL, &learner->train_dm));

  safe_call(LGBM_DatasetSetField(learner->train_dm, "label", learner->train_y,
                                 learner->n_train_samples, C_API_DTYPE_FLOAT32));
#endif
}

static void prepare_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;
  int i;

  int n_zero_samples = 0, n_zero_samples_use = 0;

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
  // create_data_holder(cache);
  if (learner->train_matrix_n_row < params->n_training_segs) {
    resize_matrix(params, &learner->train_x, &learner->train_y, &learner->train_matrix_n_row,
                  params->n_training_segs * 2);
  }

  if (learner->valid_matrix_n_row < params->n_training_segs) {
    resize_matrix(params, &learner->valid_x, &learner->valid_y, &learner->valid_matrix_n_row,
                  params->n_training_segs * 2);
  }

  // feature_t *train_x = learner->train_x;
  // train_y_t *train_y = learner->train_y;
  // feature_t *valid_x = learner->valid_x;
  // train_y_t *valid_y = learner->valid_y;

  learner->n_train_samples = learner->n_valid_samples = 0;
  segment_t *curr_seg = params->train_bucket.first_seg;

  // not all training data is useful
  // because for some of them we do not have good enough y to learn
  bool is_sample_useful;
  for (i = 0; i < params->n_training_segs / 2; i++) {
    DEBUG_ASSERT(curr_seg != NULL);

    if (learner->n_valid_samples < N_MAX_VALIDATION && next_rand() % 10 <= 0) {
      is_sample_useful = prepare_one_row(
          cache, curr_seg, true, &valid_x[learner->n_feature * learner->n_valid_samples],
          &valid_y[learner->n_valid_samples]);

      if (is_sample_useful) {
        learner->n_valid_samples += 1;
      } else {
        if (n_zero_samples != 0 && next_rand() % n_zero_samples <= 1) {
          learner->n_valid_samples += 1;
          n_zero_samples_use += 1;
        }
        n_zero_samples += 1;
      }
    } else {
      is_sample_useful = prepare_one_row(
          cache, curr_seg, true, &train_x[learner->n_feature * learner->n_train_samples],
          &train_y[learner->n_train_samples]);

      if (is_sample_useful) {
        learner->n_train_samples += 1;
      } else {
        /* do not want too much zero */
        if (n_zero_samples != 0 && next_rand() % n_zero_samples <= 1) {
          learner->n_train_samples += 1;
          n_zero_samples_use += 1;
        }
        n_zero_samples += 1;
      }
    }
    curr_seg = curr_seg->next_seg;
  }

  clean_training_segs(cache, params->n_training_segs / 2);
#else
  int pos_in_train_data = 0, pos_in_valid_data = 0;
  int copy_direction; /* 0: train, 1: validation */
  int n_feature = learner->n_feature;

  feature_t *x;
  train_y_t *y;

#if OBJECTIVE == LTR
  static train_y_t *y_sort = NULL;
  if (y_sort == NULL) y_sort = my_malloc_n(train_y_t, learner->train_matrix_n_row);

  for (i = 0; i < learner->n_train_samples; i++) {
    y_sort[i] = learner->train_y[i];
  }
  qsort(y_sort, learner->n_train_samples, sizeof(train_y_t), cmp_train_y);
  train_y_t cutoff = y_sort[learner->n_train_samples - learner->n_train_samples / 10];
#endif

  for (i = 0; i < learner->n_train_samples; i++) {
    if (learner->train_y[i] == 0) {
      n_zero_samples += 1;
    }

    copy_direction = 0;
    if (i % 10 == 0 && pos_in_valid_data < learner->valid_matrix_n_row) {
      copy_direction = 1;
    }

    if (copy_direction == 0) {
      // if (pos_in_train_data == i) {
      //   pos_in_train_data++;
      //   continue;
      // }
      x = &learner->train_x[pos_in_train_data * n_feature];
      y = &learner->train_y[pos_in_train_data];
      pos_in_train_data++;
    } else {
      x = &learner->valid_x[pos_in_valid_data * n_feature];
      y = &learner->valid_y[pos_in_valid_data];
      pos_in_valid_data++;
    }

    memcpy(x, &learner->train_x[i * n_feature], sizeof(feature_t) * n_feature);
    *y = learner->train_y[i];

#if OBJECTIVE == LTR
//      printf("%f\n", *y);
//      if (*y >= cutoff)
//        *y = 1;
//      else
//        *y = 0;
#endif
  }
  learner->n_train_samples = pos_in_train_data;
  learner->n_valid_samples = pos_in_valid_data;
#endif

  DEBUG("%.2lf hour, %d segs, %d zero samples, train:valid %d/%d\n", 
    (double) params->curr_rtime/3600.0, params->n_segs, 
    n_zero_samples, pos_in_train_data, pos_in_valid_data); 
  prepare_training_data_per_package(cache);

#ifdef DUMP_TRAINING_DATA
  {
    static __thread char filename[24];

    INFO("dump training data %d\n", learner->n_train);

    snprintf(filename, 24, "train_data_%d", learner->n_train);
    FILE *f = fopen(filename, "w");
    for (int m = 0; m < learner->n_train_samples; m++) {
      fprintf(f, "%f: ", learner->train_y[m]);
      for (int n = 0; n < learner->n_feature; n++) {
        fprintf(f, "%6f,", learner->train_x[learner->n_feature * m + n]);
      }
      fprintf(f, "\n");
    }
    fclose(f);

    snprintf(filename, 24, "valid_data_%d", learner->n_train);
    f = fopen(filename, "w");
    for (int m = 0; m < learner->n_valid_samples; m++) {
      fprintf(f, "%f: ", learner->valid_y[m]);
      for (int n = 0; n < learner->n_feature; n++) {
        fprintf(f, "%6f,", learner->valid_x[learner->n_feature * m + n]);
      }
      fprintf(f, "\n");
    }
    fclose(f);
  };
#endif
}

#ifdef USE_GBM
void train_lgbm(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

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

  LGBM_DatasetFree(tdata);
}
#endif

#ifdef USE_XGBOOST
static void train_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  prepare_training_data(cache);

  DMatrixHandle eval_dmats[2] = {learner->train_dm, learner->valid_dm};
  static const char *eval_names[2] = {"train", "valid"};
  const char *eval_result;
  double train_loss, valid_loss, last_valid_loss = 0;
  int n_stable_iter = 0;

  if (learner->n_train > 0) {
    safe_call(XGBoosterFree(learner->booster));
  }

  safe_call(XGBoosterCreate(eval_dmats, 1, &learner->booster));
  safe_call(XGBoosterSetParam(learner->booster, "booster", "gbtree"));
  //  safe_call(XGBoosterSetParam(learner->booster, "booster", "gblinear"));
  safe_call(XGBoosterSetParam(learner->booster, "verbosity", "1"));
  safe_call(XGBoosterSetParam(learner->booster, "nthread", "1"));
//  safe_call(XGBoosterSetParam(learner->booster, "eta", "0.1"));
//  safe_call(XGBoosterSetParam(learner->booster, "gamma", "1"));
#if OBJECTIVE == REG
  safe_call(XGBoosterSetParam(learner->booster, "objective", "reg:squarederror"));
#elif OBJECTIVE == LTR
  //  safe_call(XGBoosterSetParam(learner->booster, "objective", "rank:pairwise"));
  safe_call(XGBoosterSetParam(learner->booster, "objective", "rank:map"));
//  safe_call(XGBoosterSetParam(learner->booster, "objective", "rank:ndcg"));
#endif

  for (int i = 0; i < N_TRAIN_ITER; ++i) {
    // Update the model performance for each iteration
    safe_call(XGBoosterUpdateOneIter(learner->booster, i, learner->train_dm));
    if (learner->n_valid_samples < 10) continue;
    safe_call(
        XGBoosterEvalOneIter(learner->booster, i, eval_dmats, eval_names, 2, &eval_result));
#if OBJECTIVE == REG
    char *train_pos = strstr(eval_result, "train-rmse:") + 11;
    char *valid_pos = strstr(eval_result, "valid-rmse") + 11;
#elif OBJECTIVE == LTR
    char *train_pos = strstr(eval_result, "train-map") + 10;
    char *valid_pos = strstr(eval_result, "valid-map") + 10;
#else
#error
#endif
    train_loss = strtof(train_pos, NULL);
    valid_loss = strtof(valid_pos, NULL);

    // DEBUG("iter %d, train loss %.4lf, valid loss %.4lf\n", i, train_loss, valid_loss);
    if (valid_loss > 1000000) {
      printf("valid loss is too large, stop training\n");
      abort();
    }

    if (fabs(last_valid_loss - valid_loss) / valid_loss < 0.01) {
      n_stable_iter += 1;
      if (n_stable_iter > 2) {
        break;
      }
    } else {
      n_stable_iter = 0;
    }
    last_valid_loss = valid_loss;
  }

#ifndef __APPLE__
  safe_call(XGBoosterBoostedRounds(learner->booster, &learner->n_trees));
#endif

  INFO("%.2lf hour, vtime %ld, train/valid %d/%d samples, "
       "%d trees, "
       "rank intvl %.4lf\n",
      //  (unsigned long) cache->cache_size / 1024 / 1024, 
       (double) params->curr_rtime/3600, (long) params->curr_vtime,
       (int) learner->n_train_samples,
       (int) learner->n_valid_samples,
       learner->n_trees, 
       params->rank_intvl);

#ifdef DUMP_MODEL
  {
    static char s[16];
    snprintf(s, 16, "model_%d.bin", learner->n_train);
    safe_call(XGBoosterSaveModel(learner->booster, s));
    INFO("dump model %s\n", s);
  }
#endif
}
#endif

void train(cache_t *cache) {
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;
#ifdef TRAIN_ONCE
  if (params->learner.n_train > 0) return;
#endif

  uint64_t start_time = gettime_usec(); 

  train_xgboost(cache);

  uint64_t end_time = gettime_usec();
  // INFO("training time %.4lf sec\n", (end_time - start_time) / 1000000.0); 
  params->learner.n_train += 1;
  params->learner.last_train_rtime = params->curr_rtime;
  params->learner.n_train_samples = 0;
  params->learner.n_valid_samples = 0;
}



#include "learned.h"
#include "L2CacheInternal.h"

#ifdef USE_XGBOOST
#include <xgboost/c_api.h>
#define safe_call(call) {  \
  int err = (call); \
  if (err != 0) { \
    fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, XGBGetLastError());  \
    exit(1); \
  } \
}
#elif defined(USE_GBM)
#include <LightGBM/c_api.h>
#define safe_call(call)                                                                       \
  {                                                                                           \
    int err = (call);                                                                         \
    if (err != 0) {                                                                           \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, LastErrorMsg()); \
      exit(1);                                                                                \
    }                                                                                         \
  }
#endif


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
  if (learner->train_matrix_size_row < n_max_training_samples) {
    n_max_training_samples *= 2;
    if (learner->train_matrix_size_row > 0) {
      DEBUG_ASSERT(learner->training_x != NULL);
      my_free(sizeof(feature_t) * learner->train_matrix_size_row * learner->n_feature, learner->training_x);
      my_free(sizeof(train_y_t) * learner->train_matrix_size_row, learner->training_y);
    }
    learner->training_x = my_malloc_n(feature_t, n_max_training_samples * learner->n_feature);
    learner->training_y = my_malloc_n(pred_t, n_max_training_samples);
    learner->train_matrix_size_row = n_max_training_samples;
  }

  if (learner->valid_matrix_size_row < N_MAX_VALIDATION) {
    if (learner->valid_matrix_size_row > 0) {
      DEBUG_ASSERT(learner->valid_x != NULL);
      my_free(sizeof(feature_t) * learner->valid_matrix_size_row * learner->n_feature, learner->valid_x);
      my_free(sizeof(train_y_t) * learner->valid_matrix_size_row, learner->valid_y);
//      my_free(sizeof(pred_t) * learner->valid_matrix_size_row, learner->valid_pred_y);
    }
    learner->valid_x = my_malloc_n(feature_t, N_MAX_VALIDATION * learner->n_feature);
    learner->valid_y = my_malloc_n(train_y_t, N_MAX_VALIDATION);
//    learner->valid_pred_y = my_malloc_n(pred_t, N_MAX_VALIDATION);
    learner->valid_matrix_size_row = N_MAX_VALIDATION;
  }
}

void create_data_holder2(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &params->learner;

  if (learner->train_matrix_size_row == 0) {
    learner->training_x = my_malloc_n(feature_t, learner->n_max_training_segs * learner->n_feature);
    learner->training_y = my_malloc_n(pred_t, learner->n_max_training_segs);
    learner->train_matrix_size_row = learner->n_max_training_segs;

    learner->valid_matrix_size_row = learner->n_max_training_segs / 10;
    learner->valid_x = my_malloc_n(feature_t, learner->valid_matrix_size_row * learner->n_feature);
    learner->valid_y = my_malloc_n(train_y_t, learner->valid_matrix_size_row);
  }
}


static inline bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool training_data, feature_t *x, train_y_t *y) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &params->learner;

//  x[0] = x[1] = x[2] = x[3] = x[4] = x[5] = x[6] = x[7] = x[8] = x[9] = x[10] = x[11] = 0;


  x[0] = (feature_t) curr_seg->bucket_idx;
  x[1] = (feature_t) ((curr_seg->create_rtime / 3600) % 24);
  x[2] = (feature_t) ((curr_seg->create_rtime / 60) % 60);
  if (training_data)
    x[3] = (feature_t) curr_seg->become_train_seg_rtime - curr_seg->create_rtime;
  else
    x[3] = (feature_t) params->curr_rtime - curr_seg->create_rtime;
  x[4] = (feature_t) curr_seg->req_rate;
  x[5] = (feature_t) curr_seg->write_rate;
  x[6] = (feature_t) curr_seg->total_bytes / curr_seg->n_total_obj;
  x[7] = curr_seg->write_ratio;
  x[8] = curr_seg->cold_miss_ratio;
  x[9] = curr_seg->n_total_hit;
  x[10] = curr_seg->n_total_active;
  x[11] = curr_seg->n_merge;

  for (int k = 0; k < N_FEATURE_TIME_WINDOW; k++) {
    x[12 + k * 3 + 0] = (feature_t) curr_seg->feature.n_hit_per_min[k];
    x[12 + k * 3 + 1] = (feature_t) curr_seg->feature.n_hit_per_ten_min[k];
    x[12 + k * 3 + 2] = (feature_t) curr_seg->feature.n_hit_per_hour[k];


//    x[12 + k * 3 + 0] = 0;
//    x[12 + k * 3 + 1] = 0;
//    x[12 + k * 3 + 2] = 0;
  }

  if (y == NULL) {
    return true;
  }

  /* calculate y */
  double penalty = curr_seg->penalty;

  int n_retained_obj = 0;
#if TRAINING_CONSIDER_RETAIN == 1
  n_retained_obj = params->n_retain_per_seg;
#endif


#if TRAINING_TRUTH == TRAINING_Y_FROM_ORACLE
  penalty = cal_seg_penalty(cache,
                            OBJ_SCORE_ORACLE,
                            curr_seg, n_retained_obj,
                            curr_seg->become_train_seg_rtime,
                            curr_seg->become_train_seg_vtime);

#else
#endif

//  penalty = penalty * 10000000 / curr_seg->total_bytes;
#if OBJECTIVE == REG
  *y = (train_y_t) penalty;
#elif OBJECTIVE == LTR
  double rel = 1.0 / (penalty*10 + 1e-16);
  rel = 2 / (1 + exp(-rel)) - 1;
  DEBUG_ASSERT(rel >= 0 && rel <= 1);
  *y = rel;
//  printf("%lf %lf\n", penalty, rel);
#endif

  if (penalty < 0.000001) {
    return false;
  }
  return true;
}






static inline void copy_to_train_matrix(cache_t *cache, segment_t *seg) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *l = &params->learner;

  if (l->n_training_samples >= l->n_max_training_segs - 1)
    return;

  unsigned int row_idx = l->n_training_samples++;

  seg->in_training_data = true;
  seg->training_data_row_idx = row_idx;
  seg->become_train_seg_rtime = params->curr_rtime;
  seg->become_train_seg_vtime = params->curr_vtime;

  prepare_one_row(cache, seg, true,
                  &l->training_x[row_idx*l->n_feature],
                  &l->training_y[row_idx]);
//  printf("%d %f: %f %f\n", row_idx, l->training_y[row_idx],
//         l->training_x[row_idx * l->n_feature + 9],
//         l->training_x[row_idx * l->n_feature + 10]
//  );
//  print_seg(cache, seg, DEBUG_LEVEL);


}

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
         (long) params->curr_rtime, (long) params->curr_rtime/86400,
         (long) params->curr_rtime % 86400 / 3600,
         (long) params->curr_rtime % 3600,
         l->n_snapshot, l->n_training_samples, l->n_max_training_segs);
}






















static inline void prepare_training_data_per_package(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

#ifdef USE_XGBOOST
  if (params->learner.n_train > 0) {
    safe_call(XGDMatrixFree(learner->train_dm));
    safe_call(XGDMatrixFree(learner->valid_dm));
  }

  safe_call(XGDMatrixCreateFromMat(learner->training_x,
                                   learner->n_training_samples,
                                   learner->n_feature, -2, &learner->train_dm));

  safe_call(XGDMatrixCreateFromMat(learner->valid_x,
                                   learner->n_validation_samples,
                                   learner->n_feature, -2, &learner->valid_dm));

  safe_call(XGDMatrixSetFloatInfo(learner->train_dm, "label",
                                  learner->training_y,
                                  learner->n_training_samples));

  safe_call(XGDMatrixSetFloatInfo(learner->valid_dm, "label",
                                  learner->valid_y,
                                  learner->n_validation_samples));

#if OBJECTIVE == LTR
  safe_call(XGDMatrixSetUIntInfo(learner->train_dm, "group", &learner->n_training_samples, 1));
  safe_call(XGDMatrixSetUIntInfo(learner->valid_dm, "group", &learner->n_validation_samples, 1));
#endif

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

int cmp_train_y(const void *p1, const void *p2) {
  const train_y_t *d1 = p1;
  const train_y_t *d2 = p2;

  if (*d1 > * d2) {
    return 1;
  } else
    return -1;
}


static void prepare_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;
  int i;

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
  create_data_holder(cache);
#endif

  learner->n_zero_samples = learner->n_zero_samples_use = 0;

#if TRAINING_DATA_SOURCE == TRAINING_X_FROM_EVICTION
  feature_t *train_x = learner->training_x;
  train_y_t *train_y = learner->training_y;
  feature_t *valid_x = learner->valid_x;
  train_y_t *valid_y = learner->valid_y;

  learner->n_training_samples = learner->n_validation_samples = 0;
  segment_t *curr_seg = params->training_bucket.first_seg;

  bool is_sample_valid;
  for (i = 0; i < params->n_training_segs / 2; i++) {
    DEBUG_ASSERT(curr_seg != NULL);

    if (learner->n_validation_samples < N_MAX_VALIDATION && rand() % 10 <= 0) {
      is_sample_valid = prepare_one_row(
          cache, curr_seg, true,
          &valid_x[learner->n_feature * learner->n_validation_samples],
          &valid_y[learner->n_validation_samples]);

      if (is_sample_valid) {
        learner->n_validation_samples += 1;
      } else {
        if (learner->n_zero_samples != 0
            && rand() % learner->n_zero_samples <= 1) {
          learner->n_validation_samples += 1;
          learner->n_zero_samples_use += 1;
        }
        learner->n_zero_samples += 1;
      }
    } else {
      is_sample_valid = prepare_one_row(
          cache, curr_seg, true,
          &train_x[learner->n_feature * learner->n_training_samples],
          &train_y[learner->n_training_samples]);

      if (is_sample_valid) {
        learner->n_training_samples += 1;
      } else {
        /* do not want too much zero */
        if (learner->n_zero_samples != 0
            && rand() % learner->n_zero_samples <= 1) {
          learner->n_training_samples += 1;
          learner->n_zero_samples_use += 1;
        }
        learner->n_zero_samples += 1;
      }
    }
    curr_seg = curr_seg->next_seg;
  }

  clean_training_segs(cache, params->n_training_segs / 2);
#else
  int pos_in_train_data = 0, pos_in_valid_data = 0;
  bool copy = true;
  int copy_direction; /* 0: train, 1: validation */
  int n_feature = learner->n_feature;

  feature_t *x;
  train_y_t *y;

#if OBJECTIVE == LTR
  static train_y_t *y_sort = NULL;
  if (y_sort == NULL)
    y_sort = my_malloc_n(train_y_t, learner->n_max_training_segs);

  for (i = 0; i < learner->n_training_samples; i++) {
    y_sort[i] = learner->training_y[i];
  }
  qsort(y_sort, learner->n_training_samples, sizeof(train_y_t), cmp_train_y);
  train_y_t cutoff = y_sort[learner->n_training_samples - learner->n_training_samples / 10];
#endif

  for (i = 0; i < learner->n_training_samples; i++) {
    copy = true;
    if (learner->training_y[i] == 0) {
      learner->n_zero_samples += 1;
      if (learner->n_zero_samples_use != 0) {
        if (rand() % learner->n_zero_samples_use > 1) {
          copy = false;
        } else {
          learner->n_zero_samples_use += 1;
        }
      } else {
        learner->n_zero_samples_use += 1;
      }
    }

    if (copy) {
      copy_direction = 0;
      if (rand() % 10 == 0 && pos_in_valid_data < learner->valid_matrix_size_row) {
        copy_direction = 1;
      }

      if (copy_direction == 0) {
        if (pos_in_train_data == i) {
          pos_in_train_data++;
          continue;
        }
        x = &learner->training_x[pos_in_train_data * n_feature];
        y = &learner->training_y[pos_in_train_data];
        pos_in_train_data++;
      } else {
        x = &learner->valid_x[pos_in_valid_data * n_feature];
        y = &learner->valid_y[pos_in_valid_data];
        pos_in_valid_data++;
      }

      memcpy(x,
             &learner->training_x[i * n_feature],
             sizeof(feature_t) * n_feature);
      *y = learner->training_y[i];

#if OBJECTIVE == LTR
//      printf("%f\n", *y);
//      if (*y >= cutoff)
//        *y = 1;
//      else
//        *y = 0;
#endif
    }
  }
  learner->n_training_samples = pos_in_train_data;
  learner->n_validation_samples = pos_in_valid_data;
#endif


  prepare_training_data_per_package(cache);


#ifdef DUMP_TRAINING_DATA
  {
    static __thread char filename[24];

    INFO("dump training data %d\n", learner->n_train);

    snprintf(filename, 24, "train_data_%d", learner->n_train);
    FILE *f = fopen(filename, "w");
    for (int m = 0; m < learner->n_training_samples; m++) {
      fprintf(f, "%f: ", learner->training_y[m]);
      for (int n = 0; n < learner->n_feature; n++) {
        fprintf(f, "%6f,", learner->training_x[learner->n_feature * m + n]);
      }
      fprintf(f, "\n");
    }
    fclose(f);


    snprintf(filename, 24, "valid_data_%d", learner->n_train);
    f = fopen(filename, "w");
    for (int m = 0; m < learner->n_validation_samples; m++) {
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


void prepare_inference_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  if (learner->inf_matrix_size_row < params->n_segs) {
    if (learner->inf_matrix_size_row != 0) {
      DEBUG_ASSERT(learner->inference_data != NULL);
      my_free(sizeof(feature_t) * learner->inf_matrix_size_row * learner->n_feature, learner->inference_data);
      my_free(sizeof(pred_t) * learner->inf_matrix_size_row, learner->pred);
    }
    int n_row = params->n_segs * 2;
    learner->inference_data = my_malloc_n(feature_t, n_row * learner->n_feature);
    learner->pred = my_malloc_n(pred_t, n_row);
    learner->inf_matrix_size_row = n_row;
  }

  feature_t *x = learner->inference_data;


  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      prepare_one_row(cache, curr_seg, false,
                      &x[learner->n_feature * n_seg], NULL);

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
                                   learner->n_feature, -2,
                                   &learner->inf_dm));
#elif defined(USE_GBM)
  ;
#endif
}

#ifdef USE_XGBOOST
static void train_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  prepare_training_data(cache);

  DMatrixHandle eval_dmats[2] = {learner->train_dm, learner->valid_dm};
  static const char *eval_names[2] = {"train", "valid"};
  const char *eval_result;
  double l2, last_l2 = 0;
  int n_stable_iter = 0;


  if (learner->n_train == 0) {
    safe_call(XGBoosterCreate(eval_dmats, 1, &learner->booster));
  }
#ifndef ACCUMULATIVE_TRAINING
  else {
    safe_call(XGBoosterFree(learner->booster));
    safe_call(XGBoosterCreate(eval_dmats, 1, &learner->booster));
  }
#endif

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
    if (learner->n_validation_samples < 10)
      continue;
    safe_call(XGBoosterEvalOneIter(learner->booster, i, eval_dmats, eval_names, 2, &eval_result));
#if OBJECTIVE == REG
    char *pos = strstr(eval_result, "valid-rmse") + 11;
#elif OBJECTIVE == LTR
    char *pos = strstr(eval_result, "valid-map") + 10;
#else
    #error
#endif
    l2 = strtof(pos, NULL);

    DEBUG("iter %d %s %lf\n", i, eval_result, l2);
    if (l2 > 1000000)
      abort();

    if (fabs(last_l2 - l2) / l2 < 0.01) {
//    if (fabs(last_l2 - l2) / l2 < 0.01 || l2 > last_l2 * 1.05) {
      n_stable_iter += 1;
      if (n_stable_iter > 2) {
        break;
      }
    } else {
      n_stable_iter = 0;
    }
    last_l2 = l2;
  }

#ifndef __APPLE__
  safe_call(XGBoosterBoostedRounds(learner->booster, &learner->n_trees));
#endif

  INFO("cache size %lu, curr time %ld (vtime %ld) training %d segs %d samples, "
       "%d validation samples, %d/%d zeros samples, %d trees, "
       "sample every %d segs, rank intvl %d, "
       "%ld total segs \n", (unsigned long) cache->cache_size,
       (long) params->curr_rtime, (long) params->curr_vtime,
       (int) params->n_training_segs, (int) learner->n_training_samples,
       (int) learner->n_validation_samples,
       (int) learner->n_zero_samples_use, (int) learner->n_zero_samples,
       learner->n_trees,
       0,
//       params->learner.sample_every_n_seg_for_training,
       params->rank_intvl,
       (long) params->n_segs);



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


#ifdef USE_XGBOOST
void inference_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  prepare_inference_data(cache);


  /* pred result stored in xgboost lib */
  const float *pred;

#ifdef DUMP_TRAINING_DATA
  static __thread char filename[24];
  snprintf(filename, 24, "inf_data_%d", learner->n_train-1);
  FILE *f = fopen(filename, "a");
#endif

  bst_ulong out_len = 0;
  safe_call(XGBoosterPredict(learner->booster,
                             learner->inf_dm, 0, 0, 0, &out_len,
                              &pred));

  DEBUG_ASSERT(out_len == params->n_segs);

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
#if OBJECTIVE == REG
//      curr_seg->penalty = pred[n_seg];
      curr_seg->penalty = pred[n_seg] * 1e6 / curr_seg->total_bytes;
//      curr_seg->penalty = pred[n_seg] * 1e12 / curr_seg->total_bytes / curr_seg->total_bytes;
#elif OBJECTIVE == LTR
      if (pred[n_seg] < 0)
        curr_seg->penalty = 1e8;
      else
        curr_seg->penalty = 1.0 / pred[n_seg];
      printf("%lf\n", pred[n_seg]);
#endif

#ifdef DUMP_TRAINING_DATA
      fprintf(f, "%f/%lf: ", pred[n_seg], cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg,
                                        params->n_retain_per_seg,
                                        params->curr_rtime, params->curr_vtime));
      for (int j = 0; j < learner->n_feature; j++) {
        fprintf(f, "%f,", learner->inference_data[learner->n_feature * n_seg + j]);
      }
      fprintf(f, "\n");
#endif

      n_seg++;
      curr_seg = curr_seg->next_seg;
    }
  }
#ifdef DUMP_TRAINING_DATA
  fprintf(f, "\n\n\n\n");
fclose(f);
#endif
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
//                                      learner->n_validation_samples,
//                                      learner->n_feature,
//                                      1,
//                                      C_API_PREDICT_NORMAL,
//                                      0,
//                                      -1,
//                                      "linear_tree=false",
//                                      &out_len,
//                                      learner->valid_pred_y));
//
//  DEBUG_ASSERT(out_len == learner->n_validation_samples);
//  train_eval(i, learner->valid_pred_y,
//             learner->valid_y,
//             learner->n_validation_samples);
//  last_eval_time = time(NULL);
  //  }

  LGBM_DatasetFree(tdata)l;

}
#endif


#ifdef USE_GBM
void inference_lgbm(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

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
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;
#ifdef TRAIN_ONCE
  if (params->learner.n_train > 0)
    return;
#endif

  train_xgboost(cache);

  params->learner.n_train += 1;
  params->learner.last_train_rtime = params->curr_rtime;
  params->learner.n_training_samples = 0;
  params->learner.n_validation_samples = 0;
}

void inference(cache_t *cache) {
  inference_xgboost(cache);

  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;
  params->learner.n_inference += 1;
}




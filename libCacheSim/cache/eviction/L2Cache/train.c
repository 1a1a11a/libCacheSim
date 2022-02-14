

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "bucket.h"
#include "learnInternal.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"
#include "utils.h"

void transform_seg_to_training(cache_t *cache, bucket_t *bucket, segment_t *segment) {
  static __thread int n = 0, n_zero = 0;
  L2Cache_params_t *params = cache->eviction_params;
  segment->selected_for_training = true;
  /* used to calculate the eviction penalty */
  segment->become_train_seg_vtime = params->curr_vtime;
  /* used to calculate age at eviction for training */
  segment->become_train_seg_rtime = params->curr_rtime;
  segment->train_utility = 0;

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

  seg->selected_for_training = true;
  seg->training_data_row_idx = row_idx;
  seg->become_train_seg_rtime = params->curr_rtime;
  seg->become_train_seg_vtime = params->curr_vtime;
  seg->train_utility = 0;

  // TODO: do we need this?
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
        curr_seg->selected_for_training = true;
        credit -= sample_ratio;

        copy_seg_to_train_matrix(cache, curr_seg);
      } else {
        curr_seg->selected_for_training = false;
        curr_seg->train_utility = 0;// reset utility
      }

      curr_seg = curr_seg->next_seg;
    }
  }
  DEBUG("%.2lf hour cache size %.2lf MB snapshot %d/%d train sample\n",
        (double) params->curr_rtime / 3600.0, cache->cache_size / 1024.0 / 1024.0,
        l->n_train_samples, l->train_matrix_n_row);
}

/* used when the training y is calculated online */
void update_train_y(L2Cache_params_t *params, cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->L2Cache.segment;

  if (!seg->selected_for_training) {
    return;
  }

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
  // const int n_group = 20;
  // unsigned int group[n_group];
  // for (int i = 0; i < n_group; i++) {
  //   group[i] = learner->n_train_samples/n_group;
  // }
  // group[(n_group - 1)] = learner->n_train_samples - (learner->n_train_samples/n_group) * (n_group - 1);
  // safe_call(XGDMatrixSetUIntInfo(learner->train_dm, "group", group, n_group));

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
  int pos_in_train_data = 0, pos_in_valid_data = 0;
  int copy_direction; /* 0: train, 1: validation */
  int n_feature = learner->n_feature;

  feature_t *x;
  train_y_t *y;

#if OBJECTIVE == LTR
  /** convert utility to rank relevance 0 - 4, with 4 being the most relevant
      let n_candidate = learner->n_train_samples * params->rank_intvl samples 
      assume params->rank_intvl < 0.125, 
      cat 4: n_candidate, 
      cat 3: 2 * n_candidate, 
      cat 2: 3 * n_candidate,
      cat 1, 0: (n - 6 * n_candidate) / 2,
   */
  static __thread train_y_t *y_sort = NULL;
  static __thread train_y_t train_y_cutoffs[5];

  assert(params->rank_intvl <= 0.15);
  int n = learner->n_train_samples;
  int n_candidate = (int) (n * params->rank_intvl);

  if (y_sort == NULL) y_sort = my_malloc_n(train_y_t, learner->train_matrix_n_row);
  memcpy(y_sort, learner->train_y, sizeof(train_y_t) * n);
  qsort(y_sort, n, sizeof(train_y_t), cmp_train_y);

  train_y_cutoffs[4] = y_sort[n_candidate];
  train_y_cutoffs[3] = y_sort[n_candidate * 3];
  train_y_cutoffs[2] = y_sort[n_candidate * 6];
  train_y_cutoffs[1] = y_sort[n_candidate * 6 + (n - n_candidate * 6) / 2];
  train_y_cutoffs[0] = y_sort[n - 1] + 1;// to make sure the largest value is always insma
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

    // TODO: is this memcpy really necessary?
    memcpy(x, &learner->train_x[i * n_feature], sizeof(feature_t) * n_feature);
    *y = learner->train_y[i];

#if OBJECTIVE == LTR
    // convert utility to rank relevance 0 - 4, with 4 being the most relevant
    for (int p = 4; p >= 0; p--) {
      if (*y <= train_y_cutoffs[p]) {
        *y = p;
        break;
      }
    }
#endif
  }
  learner->n_train_samples = pos_in_train_data;
  learner->n_valid_samples = pos_in_valid_data;

  // DEBUG("%.2lf hour, %d segs, %d zero samples, train:valid %d/%d\n",
  //   (double) params->curr_rtime/3600.0, params->n_segs,
  //   n_zero_samples, pos_in_train_data, pos_in_valid_data);
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
  safe_call(XGBoosterSetParam(learner->booster, "objective", "rank:pairwise"));
  // safe_call(XGBoosterSetParam(learner->booster, "objective", "rank:map"));
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
#elif OBJECTIVE == LTR
    char *train_pos = strstr(eval_result, "train-map") + 10;
    char *valid_pos = strstr(eval_result, "valid-map") + 10;
    // DEBUG("%s\n", eval_result);
#else
#error
#endif
  }

#ifndef __APPLE__
  safe_call(XGBoosterBoostedRounds(learner->booster, &learner->n_trees));
#endif

  DEBUG("%.2lf hour, vtime %ld, train/valid %d/%d samples, "
        "%d trees, "
        "rank intvl %.4lf\n",
        (double) params->curr_rtime / 3600, (long) params->curr_vtime,
        (int) learner->n_train_samples, (int) learner->n_valid_samples, learner->n_trees,
        params->rank_intvl);

#ifdef DUMP_MODEL
  {
    static __thread char s[16];
    snprintf(s, 16, "model_%d.bin", learner->n_train);
    safe_call(XGBoosterSaveModel(learner->booster, s));
    INFO("dump model %s\n", s);
    abort();
  }
#endif
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

void train(cache_t *cache) {
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;

  uint64_t start_time = gettime_usec();

  train_xgboost(cache);

  uint64_t end_time = gettime_usec();
  // INFO("training time %.4lf sec\n", (end_time - start_time) / 1000000.0);
  params->learner.n_train += 1;
  params->learner.last_train_rtime = params->curr_rtime;
  params->learner.n_train_samples = 0;
  params->learner.n_valid_samples = 0;
}

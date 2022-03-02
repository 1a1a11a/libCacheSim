

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "bucket.h"
#include "learnInternal.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"
#include "utils.h"

void dump_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  static __thread char filename[24];
  snprintf(filename, 24, "train_data_%d", learner->n_train);

  INFO("dump training data %d\n", learner->n_train);

  FILE *f = fopen(filename, "w");
  fprintf(f, "# y: x\n");
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
}

/* currently we snapshot segments after each training, then we collect segment utility 
 * for the snapshotted segments over time, when it is time to retrain, we used the snapshotted segment featuers 
 * and calculated utility to train a model, Because the snapshotted segments may be evicted over time, 
 * we move evicted segments to training buckets and keep ghost entries of evicted objects so that 
 * we can more accurately calculate utility. Because we keep ghost entries, clean_training_segs is used to clean
 * up the ghost entries after each training
 */
static void clean_training_segs(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  segment_t *seg = params->train_bucket.first_seg;
  segment_t *next_seg;
  int n_cleaned = 0;

  while (seg != NULL) {
    DEBUG_ASSERT(seg != NULL);
    next_seg = seg->next_seg;
    clean_one_seg(cache, seg);
    seg = next_seg;
    n_cleaned += 1;
  }

  DEBUG_ASSERT(n_cleaned == params->n_training_segs);
  DEBUG_ASSERT(n_cleaned == params->train_bucket.n_in_use_segs);

  params->n_training_segs -= n_cleaned;
  params->train_bucket.n_in_use_segs -= n_cleaned;
  params->train_bucket.first_seg = NULL;
  params->train_bucket.last_seg = NULL;
}

/** 
 * @brief update the segment feature when a segment is evicted
 * 
 * features: 
 *    bucket_id: the bucket id of the segment
 *    create_hour: the hour when the segment is created
 *    create_min: the min when the segment is created
 *    age: the age of the segment
 *    req_rate: the request rate of the segment when it was created
 *    write_rate: the write rate of the segment when it was created
 *    mean_obj_size 
 *    miss_ratio: the miss ratio of the segment when it was created
 *    NA
 *    n_hit: the number of requests so far 
 *    n_active: the number of active objects 
 *    n_merge: the number of times it has been merged 
 *    n_hit_per_min: the number of requests per min
 *    n_hit_per_ten_min: the number of requests per 10 min
 *    n_hit_per_hour: the number of requests per hour
 * 
 * @param is_training_data: whether the data is training or inference 
 * @param x: feature vector, and we write to x 
 * @param y: label, for training  
 * 
 */
bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool is_training_data, feature_t *x,
                     train_y_t *y) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  x[0] = (feature_t) curr_seg->bucket_id;
  x[1] = (feature_t) ((curr_seg->create_rtime / 3600) % 24);
  x[2] = (feature_t) ((curr_seg->create_rtime / 60) % 60);
  if (is_training_data) {
    x[3] = (feature_t) curr_seg->become_train_seg_rtime - curr_seg->create_rtime;
    assert(curr_seg->become_train_seg_rtime == params->curr_rtime);
  } else {
    x[3] = (feature_t) params->curr_rtime - curr_seg->create_rtime;
  }
  x[4] = (feature_t) curr_seg->req_rate;
  x[5] = (feature_t) curr_seg->write_rate;
  x[6] = (feature_t) curr_seg->n_byte / curr_seg->n_obj;
  x[7] = (feature_t) curr_seg->miss_ratio;
  x[8] = 0.0;
  x[9] = curr_seg->n_hit;
  x[10] = curr_seg->n_active;
  x[11] = curr_seg->n_merge;

  for (int k = 0; k < N_FEATURE_TIME_WINDOW; k++) {
    x[12 + k * 3 + 0] = (feature_t) curr_seg->feature.n_hit_per_min[k];
    x[12 + k * 3 + 1] = (feature_t) curr_seg->feature.n_hit_per_ten_min[k];
    x[12 + k * 3 + 2] = (feature_t) curr_seg->feature.n_hit_per_hour[k];
  }

  // debug
  // x[0] = x[1] = x[2] = x[3] = x[4] = x[5] = x[6] = x[7] = x[8] = x[9] = x[10] = x[11] = 0;

  for (int i = 0; i < params->learner.n_feature; i++) {
    DEBUG_ASSERT(x[0] >= 0); 
  }

  // if (is_training_data)
  //   printf("train: ");
  // else
  //   printf("test:  ");

  // printf("%.0f/%.0f/%.0f/%.0f | %.0f, %.0f, %.0f, %.4f | %.0f/%.0f/%.0f, %.0f|%.0f|%.0f\n",
  //   x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[9], x[10], x[11], x[12], x[20], x[28]
  // );

  // for (int j = 0; j < 36; j++) {
  //   printf("%.2f, ", x[j]);
  // }
  // printf("\n");

  if (y == NULL) {
    // this is for inference and we do not calculate y
    return true;
  }

  /* calculate y for training */
  double online_utility = curr_seg->train_utility;
  double offline_utility = -1;
  *y = (train_y_t) online_utility;

  if (params->train_source_y == TRAIN_Y_FROM_ORACLE) {
    /* lower utility should be evicted first */
    offline_utility = cal_seg_utility(cache, curr_seg, true);
    *y = (train_y_t) offline_utility;
  }

  return *y > 0.000001;
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

#ifdef COMPARE_TRAINING_Y
  l->train_y_oracle[row_idx] = cal_seg_utility(cache, seg, true);
#endif

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

  double sample_ratio =
      MAX((double) params->n_in_use_segs / (double) l->train_matrix_n_row, 1.0);

  double credit = 0;// when credit reaches sample ratio, we sample a segment
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs - 1; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      credit += 1;
      if (credit >= sample_ratio && curr_seg->next_seg != NULL) {
        curr_seg->selected_for_training = true;
        credit -= sample_ratio;

        copy_seg_to_train_matrix(cache, curr_seg);
      } else {
        curr_seg->selected_for_training = false;
        /* reset utility */
        curr_seg->train_utility = 0;
      }

      curr_seg = curr_seg->next_seg;
    }
  }
}

/* used when the training y is calculated online, 
 * we calculate segment utility (for training) online in the following way: 
 * after segment is snapshotted, we calculate the segment utility correspond to the time 
 * when the snapshot was taken:
 * each time when an object on the segment is requested, we accumulate 1/(D_snapshot * S_obj) to the segment utility
 */
void update_train_y(L2Cache_params_t *params, cache_obj_t *cache_obj) {
  segment_t *seg = cache_obj->L2Cache.segment;

  if (params->train_source_y == TRAIN_Y_FROM_ORACLE) return;// do nothing

#ifdef EVICTION_CONSIDER_RETAIN
  if (seg->n_seg_util_skipped++ >= params->n_retain_per_seg)
#endif
  {
    double age = (double) params->curr_vtime - seg->become_train_seg_vtime;
    seg->train_utility += 1.0e6 / age / cache_obj->obj_size;
    params->learner.train_y[seg->training_data_row_idx] = seg->train_utility;
  }
}

static int cmp_train_y(const void *p1, const void *p2) {
  const train_y_t *d1 = p1;
  const train_y_t *d2 = p2;

  if (*d1 > *d2) {
    return 1;
  } else
    return -1;
}

static void prepare_training_data_per_package(cache_t *cache) {
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
  // set group used for MAP and NDCG
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

void prepare_training_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;
  int i;

  int n_zero_samples = 0, n_zero_samples_use = 0;
  int pos_in_train_data = 0, pos_in_valid_data = 0;
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

  bool use_for_validation; /* 0: train, 1: validation */
  for (i = 0; i < learner->n_train_samples; i++) {
    if (learner->train_y[i] == 0) {
      n_zero_samples += 1;
    }

    use_for_validation = (i % 10 == 0 && pos_in_valid_data < learner->valid_matrix_n_row);

    if (!use_for_validation) {
      x = &learner->train_x[pos_in_train_data * n_feature];
      y = &learner->train_y[pos_in_train_data];
      pos_in_train_data++;
    } else {
      x = &learner->valid_x[pos_in_valid_data * n_feature];
      y = &learner->valid_y[pos_in_valid_data];
      pos_in_valid_data++;
    }

    // TODO: optimize memcpy by splitting into train and valid in snapshot
    memcpy(x, &learner->train_x[i * n_feature], sizeof(feature_t) * n_feature);
    *y = learner->train_y[i];

#ifdef COMPARE_TRAINING_Y
    fprintf(ofile_cmp_y, "%lf, %lf\n", learner->train_y[i], learner->train_y_oracle[i]);
#endif

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
#ifdef COMPARE_TRAINING_Y
  fprintf(ofile_cmp_y, "#####################################\n");
#endif

  learner->n_train_samples = pos_in_train_data;
  learner->n_valid_samples = pos_in_valid_data;

  prepare_training_data_per_package(cache);

#ifdef DUMP_TRAINING_DATA
  dump_training_data(cache);
#endif

  clean_training_segs(cache);
}

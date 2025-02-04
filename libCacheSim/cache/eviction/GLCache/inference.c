

#include "GLCacheInternal.h"
#include "const.h"
#include "obj.h"
#include "utils.h"

static inline void resize_matrix(GLCache_params_t *params, feature_t **x_p,
                                 pred_t **y_p, int32_t *size_p,
                                 int64_t new_size) {
  learner_t *learner = &params->learner;
  if ((*size_p) != 0) {
    // free previously allocated memory
    // TODO: use realloc instead of free
    DEBUG_ASSERT(*x_p != NULL);
    my_free(sizeof(feature_t) * (*size_p) * learner->n_feature, *x_p);
    my_free(sizeof(pred_t) * (*size_p), *y_p);
  }

  *x_p = my_malloc_n(feature_t, new_size * learner->n_feature);
  memset(*x_p, 0, sizeof(feature_t) * new_size * learner->n_feature);
  *y_p = my_malloc_n(pred_t, new_size);
  memset(*y_p, 0, sizeof(pred_t) * new_size);
  *size_p = new_size;
}

/* calculate the ranking of all segments for eviction */
/* TODO: can sample some segments to improve throughput */
static int prepare_inference_data(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  /** because each segment is not fixed size,
   * the number of segments in total can vary over time,
   * if the inference data matrix is not big enough,
   * we resize the matrix */
  if (learner->inf_matrix_n_row < params->n_in_use_segs) {
    resize_matrix(params, &learner->inference_x, &learner->pred,
                  &learner->inf_matrix_n_row, params->n_in_use_segs * 2);
  }

  feature_t *x = learner->inference_x;

  unsigned int n_segs = 0;
  int inv_sample_ratio = MAX(params->n_in_use_segs / N_INFERENCE_DATA, 1);
  int credit = inv_sample_ratio;
  // printf("%d %d\n", params->n_in_use_segs, inv_sample_ratio);

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
      if (--credit == 0) {
        prepare_one_row(cache, curr_seg, false, &x[learner->n_feature * n_segs],
                        NULL);
        curr_seg = curr_seg->next_seg;
        n_segs++;

        credit = inv_sample_ratio;
      }
    }
  }
  DEBUG_ASSERT(inv_sample_ratio > 1 || params->n_in_use_segs == n_segs);

  if (params->learner.n_inference > 0) {
    safe_call(XGDMatrixFree(learner->inf_dm));
  }
  safe_call(XGDMatrixCreateFromMat(learner->inference_x, n_segs,
                                   learner->n_feature, -2, &learner->inf_dm));
  
  // convert to array interface format
  char str_n_segs[128];
  snprintf(str_n_segs, sizeof(str_n_segs),
           "{\"data\": [%llu],\"shape\": [1],\"typestr\": \"<u4\",\"version\": 3}",
           (unsigned long long)(uintptr_t)&n_segs);
  safe_call(XGDMatrixSetInfoFromInterface(learner->inf_dm, "group", str_n_segs));

  return n_segs;
}

void inference_xgboost(cache_t *cache) {
  GLCache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  int n_segs = prepare_inference_data(cache);

  /* pred result stored in xgboost lib */
  const float *pred;

#ifdef DUMP_INFERENCE_DATA
  static __thread char filename[24];
  snprintf(filename, 24, "inf_data_%d", learner->n_train - 1);
  FILE *f = fopen(filename, "a");
#endif

  bst_ulong out_len = 0;
  // TODO: use XGBoosterPredictFromDense to avoid copy
  // https://github.com/dmlc/xgboost/blob/36346f8f563ef79bae94604e60483fb0bf4c2661/demo/c-api/inference/inference.c
  safe_call(XGBoosterPredict(learner->booster, learner->inf_dm, 0, 0, 0,
                             &out_len, &pred));
  DEBUG_ASSERT(out_len == n_segs);

  segment_t **ranked_segs = params->seg_sel.ranked_segs;

  n_segs = 0;
  int inv_sample_ratio = MAX(params->n_in_use_segs / N_INFERENCE_DATA, 1);
  int credit = inv_sample_ratio;

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
      if (--credit == 0) {
        credit = inv_sample_ratio;
#if OBJECTIVE == REG
        if (pred[n_segs] > 0)
          curr_seg->pred_utility = pred[n_segs] * 1e6 / curr_seg->n_byte;
        else if (pred[n_segs] < 0)
          curr_seg->pred_utility = pred[n_segs] * curr_seg->n_byte;
#elif OBJECTIVE == LTR
        // segments with smaller utility (high relevance) are evicted first
        if (pred[n_segs] > 0) {
          curr_seg->pred_utility = 1.0 / pred[n_segs];
        } else {
          curr_seg->pred_utility = INT32_MAX;
        }
#endif

        if (params->buckets[curr_seg->bucket_id].n_in_use_segs <
            params->n_merge + 1) {
          // if the segment is the last segment of a bucket or the bucket does
          // not have enough segments
          curr_seg->pred_utility += INT32_MAX / 2;
        }

        ranked_segs[n_segs] = curr_seg;
        n_segs++;

      } else {
        curr_seg->pred_utility = INT32_MAX;
      }

#ifdef DUMP_INFERENCE_DATA
      fprintf(f, "%d %f/%lf: ", n_segs, pred[n_segs],
              cal_seg_utility(cache, curr_seg, true));
      for (int j = 0; j < learner->n_feature; j++) {
        fprintf(f, "%f,",
                learner->inference_x[learner->n_feature * n_segs + j]);
      }
      fprintf(f, "\n");
#endif

      curr_seg = curr_seg->next_seg;
    }
  }

  params->seg_sel.n_ranked_segs = n_segs;

#ifdef DUMP_INFERENCE_DATA
  fprintf(f, "\n\n\n\n");
  fclose(f);
#endif
}

void inference(cache_t *cache) {
  GLCache_params_t *params = (GLCache_params_t *)cache->eviction_params;

  uint64_t start_time = gettime_usec();

  inference_xgboost(cache);

  uint64_t end_time = gettime_usec();
  // INFO("inference time %.4lf sec\n", (end_time - start_time) / 1000000.0);

  params->learner.n_inference += 1;
}

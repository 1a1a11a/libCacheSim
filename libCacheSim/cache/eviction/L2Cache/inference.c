

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "bucket.h"
#include "const.h"
#include "learnInternal.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"
#include "utils.h"

/* calculate the ranking of all segments for eviction */
/* TODO: can sample some segments to improve throughput */
void prepare_inference_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  /** because each segment is not fixed size, 
   * the number of segments in total can vary over time, 
   * if the inference data matrix is not big enough, 
   * we resize the matrix */
  if (learner->inf_matrix_n_row < params->n_in_use_segs) {
    resize_matrix(params, &learner->inference_x, &learner->pred, &learner->inf_matrix_n_row,
                  params->n_in_use_segs * 2);
  }

  feature_t *x = learner->inference_x;

  int n_segs = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
      prepare_one_row(cache, curr_seg, false, &x[learner->n_feature * n_segs], NULL);
      curr_seg = curr_seg->next_seg;
      n_segs++;
    }
  }
  DEBUG_ASSERT(params->n_in_use_segs == n_segs);

#ifdef USE_XGBOOST
  if (params->learner.n_inference > 0) {
    safe_call(XGDMatrixFree(learner->inf_dm));
  }
  safe_call(XGDMatrixCreateFromMat(learner->inference_x, params->n_in_use_segs,
                                   learner->n_feature, -2, &learner->inf_dm));

  safe_call(XGDMatrixSetUIntInfo(learner->inf_dm, "group", &params->n_in_use_segs, 1));
#elif defined(USE_GBM)
#error
#endif
}

#ifdef USE_XGBOOST
void inference_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &params->learner;

  prepare_inference_data(cache);

  /* pred result stored in xgboost lib */
  const float *pred;

#ifdef DUMP_TRAINING_DATA
  static __thread char filename[24];
  snprintf(filename, 24, "inf_data_%d", learner->n_train - 1);
  FILE *f = fopen(filename, "w");
#endif

  bst_ulong out_len = 0;
  safe_call(XGBoosterPredict(learner->booster, learner->inf_dm, 0, 0, 0, &out_len, &pred));
  DEBUG_ASSERT(out_len == params->n_in_use_segs);

  int n_segs = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
#if OBJECTIVE == REG
      // curr_seg->pred_utility = pred[n_segs];
      curr_seg->pred_utility = pred[n_segs] * 1e6 / curr_seg->n_byte;
#elif OBJECTIVE == LTR
      // segments with smaller utility (high relevance) are evicted first
      if (pred[n_segs] > 0) {
        curr_seg->pred_utility = 1.0 / pred[n_segs];
      } else {
        curr_seg->pred_utility = INT32_MAX;
      }
#endif
      // PRINT_N_TIMES(8, "%d y_hat %f, features: %.0f %.0f %.0f %.2f %.2f %.2f %.2f\n",
      //   n_segs, pred[n_segs],
      //   learner->inference_x[learner->n_feature * n_segs + 0],
      //   learner->inference_x[learner->n_feature * n_segs + 1],
      //   learner->inference_x[learner->n_feature * n_segs + 2],
      //   learner->inference_x[learner->n_feature * n_segs + 3],
      //   learner->inference_x[learner->n_feature * n_segs + 4],
      //   learner->inference_x[learner->n_feature * n_segs + 5],
      //   learner->inference_x[learner->n_feature * n_segs + 6]
      // );

#ifdef DUMP_TRAINING_DATA
      fprintf(f, "%f/%lf: ", pred[n_segs], cal_seg_utility(cache, curr_seg, true));
      for (int j = 0; j < learner->n_feature; j++) {
        fprintf(f, "%f,", learner->inference_x[learner->n_feature * n_segs + j]);
      }
      fprintf(f, "\n");
#endif

      n_segs++;
      curr_seg = curr_seg->next_seg;
    }
  }
#ifdef DUMP_TRAINING_DATA
  fprintf(f, "\n\n\n\n");
  fclose(f);
#endif
}
#endif

void inference(cache_t *cache) {
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;

  uint64_t start_time = gettime_usec();

  inference_xgboost(cache);

  uint64_t end_time = gettime_usec();
  // INFO("inference time %.4lf sec\n", (end_time - start_time) / 1000000.0);

  params->learner.n_inference += 1;
}

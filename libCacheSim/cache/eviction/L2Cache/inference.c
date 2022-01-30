

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "learnInternal.h"
#include "bucket.h"
#include "obj.h"
#include "segment.h"
#include "learned.h"
#include "const.h"


void prepare_inference_data(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  if (learner->inf_matrix_row_len < params->n_segs) {
    if (learner->inf_matrix_row_len != 0) {
      DEBUG_ASSERT(learner->inference_data != NULL);
      my_free(sizeof(feature_t) * learner->inf_matrix_row_len * learner->n_feature,
              learner->inference_data);
      my_free(sizeof(pred_t) * learner->inf_matrix_row_len, learner->pred);
    }
    int n_row = params->n_segs * 2;
    learner->inference_data = my_malloc_n(feature_t, n_row * learner->n_feature);
    learner->pred = my_malloc_n(pred_t, n_row);
    learner->inf_matrix_row_len = n_row;
  }

  feature_t *x = learner->inference_data;

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
      DEBUG_ASSERT(curr_seg != NULL);
      prepare_one_row(cache, curr_seg, false, &x[learner->n_feature * n_seg], NULL);

      curr_seg = curr_seg->next_seg;
      n_seg++;
    }
  }
  DEBUG_ASSERT(params->n_segs == n_seg);

#ifdef USE_XGBOOST
  if (params->learner.n_inference > 0) {
    safe_call(XGDMatrixFree(learner->inf_dm));
  }
  safe_call(XGDMatrixCreateFromMat(learner->inference_data, params->n_segs, learner->n_feature,
                                   -2, &learner->inf_dm));
#elif defined(USE_GBM)
  ;
#endif
}


#ifdef USE_GBM
void inference_lgbm(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  prepare_inference_data(cache);

  int64_t out_len = 0;
  safe_call(LGBM_BoosterPredictForMat(learner->booster, learner->inference_data,
                                      C_API_DTYPE_FLOAT64, params->n_segs, learner->n_feature,
                                      1, C_API_PREDICT_NORMAL, 0, -1, "num_threads=1", &out_len,
                                      learner->pred));

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


#ifdef USE_XGBOOST
void inference_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;

  learner_t *learner = &((L2Cache_params_t *) cache->eviction_params)->learner;

  prepare_inference_data(cache);

  /* pred result stored in xgboost lib */
  const float *pred;

#ifdef DUMP_TRAINING_DATA
  static __thread char filename[24];
  snprintf(filename, 24, "inf_data_%d", learner->n_train - 1);
  FILE *f = fopen(filename, "a");
#endif

  bst_ulong out_len = 0;
  safe_call(XGBoosterPredict(learner->booster, learner->inf_dm, 0, 0, 0, &out_len, &pred));

  DEBUG_ASSERT(out_len == params->n_segs);

  int n_seg = 0;
  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_seg; si++) {
#if OBJECTIVE == REG
      //      curr_seg->penalty = pred[n_seg];
      curr_seg->penalty = pred[n_seg] * 1e6 / curr_seg->n_byte;
//      curr_seg->penalty = pred[n_seg] * 1e12 / curr_seg->n_byte / curr_seg->n_byte;
#elif OBJECTIVE == LTR
      if (pred[n_seg] < 0) curr_seg->penalty = 1e8;
      else
        curr_seg->penalty = 1.0 / pred[n_seg];
      // printf("%lf\n", pred[n_seg]);
#endif

#ifdef DUMP_TRAINING_DATA
      fprintf(f, "%f/%lf: ", pred[n_seg],
              cal_seg_penalty(cache, OBJ_SCORE_ORACLE, curr_seg, params->n_retain_per_seg,
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


void inference(cache_t *cache) {
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;

  inference_xgboost(cache);

  params->learner.n_inference += 1;
}

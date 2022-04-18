

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
static int prepare_inference_data(cache_t *cache) {
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
  int inv_sample_ratio = MAX(params->n_in_use_segs / N_INFERENCE_DATA, 1); 
  int credit = inv_sample_ratio; 
  // printf("%d %d\n", params->n_in_use_segs, inv_sample_ratio);

  for (int bi = 0; bi < MAX_N_BUCKET; bi++) {
    segment_t *curr_seg = params->buckets[bi].first_seg;
    for (int si = 0; si < params->buckets[bi].n_in_use_segs; si++) {
      if (--credit == 0) {
        prepare_one_row(cache, curr_seg, false, &x[learner->n_feature * n_segs], NULL);
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

  safe_call(XGDMatrixSetUIntInfo(learner->inf_dm, "group", &n_segs, 1));

  return n_segs; 
}

void inference_xgboost(cache_t *cache) {
  L2Cache_params_t *params = cache->eviction_params;
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
  // TODO: use XGBoosterPredictFromDense to avoid copy https://github.com/dmlc/xgboost/blob/36346f8f563ef79bae94604e60483fb0bf4c2661/demo/c-api/inference/inference.c 
  safe_call(XGBoosterPredict(learner->booster, learner->inf_dm, 0, 0, 0, &out_len, &pred));
  DEBUG_ASSERT(out_len == n_segs);

  // this is slower than using XGBoosterPredict
  // char const config[] = "{\"type\": 0, \"iteration_begin\": 0, "
  //                       "\"iteration_end\": 0, \"strict_shape\": true, "
  //                       "\"cache_id\": 0, \"missing\": NaN}";
  // static __thread char array_interface[128]; 
  // snprintf(array_interface, 128, "{\"data\": [%lu, true], \"shape\": [%d, %d], "
  //                       "\"typestr\": \"<f4\", \"version\": 3}", 
  //                       (size_t) learner->inference_x, n_segs, learner->n_feature);
  // uint64_t const *out_shape;
  // uint64_t out_dim;

  // safe_call(XGBoosterPredictFromDense(learner->booster, array_interface, config, NULL,
  //                                         &out_shape, &out_dim, &pred));

  // if (out_dim != 2 || out_shape[0] != n_segs || out_shape[1] != 1) {
  //   fprintf(stderr,
  //           "Regression model should output prediction as vector, %lu, %lu",
  //           out_dim, out_shape[0]);
  //   exit(-1);
  // }


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

        if (params->buckets[curr_seg->bucket_id].n_in_use_segs < params->n_merge + 1) {
          // if the segment is the last segment of a bucket or the bucket does not have enough segments
          curr_seg->pred_utility += INT32_MAX / 2;
        }

        ranked_segs[n_segs] = curr_seg;
        n_segs++;

      } else {
        curr_seg->pred_utility = INT32_MAX;
      }

#ifdef DUMP_INFERENCE_DATA
      fprintf(f, "%d %f/%lf: ", n_segs, pred[n_segs], cal_seg_utility(cache, curr_seg, true));
      for (int j = 0; j < learner->n_feature; j++) {
        fprintf(f, "%f,", learner->inference_x[learner->n_feature * n_segs + j]);
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
  L2Cache_params_t *params = (L2Cache_params_t *) cache->eviction_params;

  uint64_t start_time = gettime_usec();

  inference_xgboost(cache);

  uint64_t end_time = gettime_usec();
  // INFO("inference time %.4lf sec\n", (end_time - start_time) / 1000000.0);

  params->learner.n_inference += 1;
}

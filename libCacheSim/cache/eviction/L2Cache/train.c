

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "L2CacheInternal.h"
#include "bucket.h"
#include "learnInternal.h"
#include "learned.h"
#include "obj.h"
#include "segment.h"
#include "utils.h"


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

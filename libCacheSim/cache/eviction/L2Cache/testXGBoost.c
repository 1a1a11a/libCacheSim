//
// Created by Juncheng Yang on 5/11/21.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <xgboost/c_api.h>

#define N_FEATURE 16
#define N_TRAINING_SAMPLES 8000
#define N_TEST_SAMPLES 200
#define OBJECTIVE LTR
#define N_TRAIN_ITER 10

#define safe_call(call)                                                                        \
  {                                                                                            \
    int err = (call);                                                                          \
    if (err != 0) {                                                                            \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call,                   \
              XGBGetLastError());                                                              \
      exit(1);                                                                                 \
    }                                                                                          \
  }

static float train_x[N_TRAINING_SAMPLES * N_FEATURE];
static float train_y[N_TRAINING_SAMPLES];
static float test_x[N_TEST_SAMPLES * N_FEATURE];
// static float test_y[N_TEST_SAMPLES];

static BoosterHandle booster; // model
static DMatrixHandle train_dm;// training data
static DMatrixHandle inf_dm;  // inference data

void prepare_data() {
  for (int i = 0; i < N_TRAINING_SAMPLES; i++) {
    for (int j = 0; j < N_FEATURE; j++) {
      train_x[i * N_FEATURE + j] = rand() % 100;
    }
    train_y[i] = i % 10;
  }

  for (int i = 0; i < N_TEST_SAMPLES; i++) {
    for (int j = 0; j < N_FEATURE; j++) {
      test_x[i * N_FEATURE + j] = rand() % 100;
    }
  }

  int n_train_samples = N_TRAINING_SAMPLES;
  int n_feature = N_FEATURE;
  int n_test_samples = N_TEST_SAMPLES;

  safe_call(XGDMatrixCreateFromMat(train_x, n_train_samples, n_feature, -2, &train_dm));
  safe_call(XGDMatrixSetFloatInfo(train_dm, "label", train_y, n_train_samples));
  safe_call(XGDMatrixSetUIntInfo(train_dm, "group", &n_train_samples, 1));

  safe_call(XGDMatrixCreateFromMat(test_x, n_test_samples, n_feature, -2, &inf_dm));
  safe_call(XGDMatrixSetUIntInfo(inf_dm, "group", &n_test_samples, 1));
}

void train() {
  DMatrixHandle eval_dmats[1] = {train_dm};
  static const char *eval_names[1] = {"train"};
  const char *eval_result;
  double train_loss;

  safe_call(XGBoosterCreate(eval_dmats, 1, &booster));
  safe_call(XGBoosterSetParam(booster, "booster", "gbtree"));
  safe_call(XGBoosterSetParam(booster, "verbosity", "1"));
  safe_call(XGBoosterSetParam(booster, "nthread", "1"));
//  safe_call(XGBoosterSetParam(booster, "eta", "0.1"));
//  safe_call(XGBoosterSetParam(booster, "gamma", "1"));
#if OBJECTIVE == REG
  safe_call(XGBoosterSetParam(booster, "objective", "reg:squarederror"));
#elif OBJECTIVE == LTR
   safe_call(XGBoosterSetParam(booster, "objective", "rank:pairwise"));
  // safe_call(XGBoosterSetParam(booster, "objective", "rank:map"));
//   safe_call(XGBoosterSetParam(booster, "objective", "rank:ndcg"));
#endif

  for (int i = 0; i < N_TRAIN_ITER; ++i) {
    safe_call(XGBoosterUpdateOneIter(booster, i, train_dm));
    safe_call(XGBoosterEvalOneIter(booster, i, eval_dmats, eval_names, 1, &eval_result));
    printf("%s\n", eval_result);
  }
}

void inference() {
  const float *pred;
  bst_ulong out_len = 0;
  safe_call(XGBoosterPredict(booster, inf_dm, 0, 0, 0, &out_len, &pred));

  int n_segs = 0;
  for (int i = 0; i < 8; i++) {
    printf("%f\n", pred[i]);
  }
}

int main() {

  prepare_data();
  train();
  inference();
  return 0;
}
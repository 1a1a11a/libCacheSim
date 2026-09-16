//
// S4FIFO_predictor.c - see S4FIFO_predictor.h.
//
// prepare_model_input()'s feature layout/derivations and kS4FIFOConfigs are
// ported verbatim from williamnixon20/CacheLib@72bb8103
// cachelib/allocator/S4FIFOLightGBMPredictor.h - do not "clean up" the
// formulas here without retraining the model, the two must match exactly.
//

#include "S4FIFO_predictor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "S4FIFO_model_real.h"
#include "libCacheSim/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#define S4FIFO_MODEL_N_FEATURES 75
// what the .s4m models are fed - see s4fifo_prepare_model_input76()
#define S4FIFO_REAL_MODEL_N_FEATURES 76
#define S4FIFO_PREDICT_MIN_REQUESTS 10000
#define S4FIFO_PREDICT_MIN_HITS 100

// The 20 configurations every model chooses among. A model's output is an
// index into this table, so the two must match exactly.
static const S4FIFOConfigEntry kS4FIFOConfigs[S4FIFO_MODEL_N_CONFIGS] = {
    {0.20, 1, 0, 0.9, 0.25},    // Class 0
    {0.04, 2, 0, 0.6, 0.80},    // Class 1
    {0.30, 1, 0, 6.0, 0.25},    // Class 2
    {0.03, 3, 0, 1.5, 0.20},    // Class 3
    {0.015, 50, 0, 0.3, 0.80},  // Class 4
    {0.08, 10, 0, 2.0, 0.00},   // Class 5
    {0.16, 6, 0, 4.0, 0.35},    // Class 6
    {0.70, 1, 1, 0.9, 0.25},    // Class 7
    {0.05, 2, 0, 6.0, 0.25},    // Class 8
    {0.05, 1, 0, 0.9, 0.25},    // Class 9
    {0.20, 1, 0, 3.0, 0.25},    // Class 10
    {0.50, 1, 0, 0.9, 0.25},    // Class 11
    {0.13, 8, 0, 8.0, 0.25},    // Class 12
    {0.10, 20, 0, 1.5, 0.80},   // Class 13
    {0.03, 5, 0, 6.0, 1.00},    // Class 14
    {0.03, 5, 0, 1.2, 0.05},    // Class 15
    {0.015, 2, 0, 20.0, 0.05},  // Class 16
    {0.80, 1, 0, 0.0, 0.05},    // Class 17
    {0.03, 6, 0, 1.5, 0.40},    // Class 18
    {0.30, 8, 0, 3.0, 0.90},    // Class 19
};

// Histogram bins are named hist_{queue}_0 .. hist_{queue}_19 and the model
// was trained on features sorted ALPHABETICALLY by name, so bin order in
// the model's input is the lexicographic order of "0".."19", not numeric
// order: 0, 1, 10, 11, ..., 19, 2, 3, ..., 9.
static const int kHistOrder[20] = {0,  1,  10, 11, 12, 13, 14, 15, 16, 17,
                                   18, 19, 2,  3,  4,  5,  6,  7,  8,  9};

// Feature order (alphabetical, matching training):
// 0: H_g, 1: H_m, 2: H_s, 3: decay_rate_small, 4: entropy_gap,
// 5: ghost_pressure, 6-25: hist_ghost_0..19, 26-45: hist_main_0..19,
// 46-65: hist_small_0..19, 66: log_C, 67: probation_efficiency,
// 68: ratio_estimate, 69: rho_onehit, 70: rho_unique, 71: scan_intensity,
// 72: tail_heaviness, 73: thrashing_risk, 74: total_reqs
static void s4fifo_prepare_model_input(const S4FIFO_feature_vector_t *fv,
                                       double *input /* [75] */) {
  double log_c = fv->log_cache_capacity;

  double probation_efficiency = (double)fv->hits_small / (fv->hits_main + 1e-6);
  double ghost_pressure =
      (double)fv->hits_ghost / ((double)fv->total_hits + fv->hits_ghost + 1e-6);
  double entropy_gap = fv->hit_ratio_main - fv->hit_ratio_small;
  double decay_rate_small = fv->hist_small[0] - fv->hist_small[1];

  double tail_heaviness = 0.0;
  for (int i = 10; i < 20; i++) tail_heaviness += fv->hist_main[i];

  double cache_size = pow(10.0, log_c);
  double working_set_size = (double)fv->total_requests * fv->unique_ratio;
  double ratio_estimate = cache_size / (working_set_size + 1e-6);
  if (ratio_estimate < 0.0001) ratio_estimate = 0.0001;
  if (ratio_estimate > 1.0) ratio_estimate = 1.0;

  double thrashing_risk = fv->unique_ratio / (ratio_estimate * 100.0 + 1e-6);
  double scan_intensity = fv->one_hit_ratio * (1.0 - ratio_estimate);

  input[0] = fv->hit_ratio_ghost;
  input[1] = fv->hit_ratio_main;
  input[2] = fv->hit_ratio_small;
  input[3] = decay_rate_small;
  input[4] = entropy_gap;
  input[5] = ghost_pressure;

  for (int i = 0; i < 20; i++) input[6 + i] = fv->hist_ghost[kHistOrder[i]];
  for (int i = 0; i < 20; i++) input[26 + i] = fv->hist_main[kHistOrder[i]];
  for (int i = 0; i < 20; i++) input[46 + i] = fv->hist_small[kHistOrder[i]];

  input[66] = log_c;
  input[67] = probation_efficiency;
  input[68] = ratio_estimate;
  input[69] = fv->one_hit_ratio;
  input[70] = fv->unique_ratio;
  input[71] = scan_intensity;
  input[72] = tail_heaviness;
  input[73] = thrashing_risk;
  input[74] = (double)fv->total_requests;
}

// Same feature engineering as s4fifo_prepare_model_input(), minus the two
// entries (log_C, ratio_estimate) the real model's feature set doesn't
// include - see model_metadata.json's feature_columns on the HF Space this
// was ported from. Re-derives the 75-feature vector and drops indices 66
// and 68 rather than duplicating the derivation math, so the two backends
// can never silently drift apart on the features they share.
static void s4fifo_prepare_model_input73(const S4FIFO_feature_vector_t *fv,
                                         double *input73 /* [73] */) {
  double input75[S4FIFO_MODEL_N_FEATURES];
  s4fifo_prepare_model_input(fv, input75);
  int j = 0;
  for (int i = 0; i < S4FIFO_MODEL_N_FEATURES; i++) {
    if (i == 66 || i == 68) continue;  // log_C, ratio_estimate
    input73[j++] = input75[i];
  }
}

// The 73-feature input PLUS 3 within-window trend features appended at the
// end (indices 73-75) - see S4FIFO_feature_vector_t's hit_ratio_trend/
// unique_ratio_trend/small_share_trend. Appending (rather than re-sorting
// into the existing alphabetical layout) means a model exported before
// these existed still loads and predicts correctly against this same
// buffer: its trees only ever reference feature_idx < 73, so the 3 extra
// trailing entries are simply never read.
static void s4fifo_prepare_model_input76(const S4FIFO_feature_vector_t *fv,
                                         double *input76 /* [76] */) {
  s4fifo_prepare_model_input73(fv, input76);
  input76[73] = fv->hit_ratio_trend;
  input76[74] = fv->unique_ratio_trend;
  input76[75] = fv->small_share_trend;
}

// Guards both backends: not enough data collected yet for a meaningful
// prediction (see S4FIFO_predictor.h).
static bool s4fifo_has_enough_data(const S4FIFO_feature_vector_t *fv) {
  return fv->total_requests >= S4FIFO_PREDICT_MIN_REQUESTS &&
         fv->total_hits >= S4FIFO_PREDICT_MIN_HITS;
}

bool s4fifo_predict(const S4FIFO_feature_vector_t *fv, S4FIFOConfigEntry *out) {
  if (!s4fifo_has_enough_data(fv)) {
    return false;
  }

  double input76[S4FIFO_REAL_MODEL_N_FEATURES];
  s4fifo_prepare_model_input76(fv, input76);
  return s4fifo_real_model_predict(input76, out);
}

const S4FIFOConfigEntry *s4fifo_get_config_table(void) {
  return kS4FIFOConfigs;
}

bool s4fifo_dump_features76(const S4FIFO_feature_vector_t *fv, double *out76) {
  if (!s4fifo_has_enough_data(fv)) {
    return false;
  }
  s4fifo_prepare_model_input76(fv, out76);
  return true;
}

bool s4fifo_predict_auto(const S4FIFO_feature_vector_t *fv,
                         const char *model_path, S4FIFOConfigEntry *out) {
  if (!s4fifo_has_enough_data(fv)) {
    return false;
  }

  if (model_path != NULL && model_path[0] != '\0' &&
      !s4fifo_real_model_load(model_path)) {
    WARN_ONCE(
        "S4FIFO: failed to load model at %s, "
        "falling back to the compiled-in default model\n",
        model_path);
  }

  // whichever model s4fifo_real_model_load() left active
  return s4fifo_predict(fv, out);
}

#ifdef __cplusplus
}
#endif

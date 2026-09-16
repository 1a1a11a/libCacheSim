//
// S4FIFO_predictor.h - learned control plane for S4FIFO ("auto-tune=1").
//
// Given a snapshot of cache-level features collected by S4FIFO_features.h,
// predicts a full 5-knob S4FIFO configuration, choosing among the 20
// candidate configurations (see S4FIFOConfigEntry). Both model sources run
// through the same evaluator (S4FIFO_model_real.h):
//
//   - the compiled-in default (model.h - 1460 trees,
//     transcribed from its .s4m by scripts/s4fifo_embed_model.py), used
//     with no setup at all.
//   - any other .s4m via `model-path=/path/to/model.s4m`, for models too
//     large to vendor. Loaded from an external file at runtime.
//
// Only compiled in when built with -DENABLE_S4FIFO=ON.
//

#pragma once

#include <stdbool.h>

#include "S4FIFO_features.h"

#ifdef __cplusplus
extern "C" {
#endif

// 20 representative configs from the tail-risk-aware greedy set-cover over
// the v3 182-candidate pool (see greedy_setcover.py / setcover_selection.csv)
// - supersedes the original 18-class table ported from
// williamnixon20/CacheLib, which was scoped to the since-removed lite model.
#define S4FIFO_MODEL_N_CONFIGS 20

// One candidate S4FIFO configuration. Field names/types match S4FIFO.c's own
// parameters directly so a prediction can be applied without conversion.
typedef struct {
  double small_size_ratio;
  int move_to_main_threshold;
  int ghost_to_main_threshold;
  double ghost_size_ratio;
  double small_skip_ratio;
} S4FIFOConfigEntry;

/**
 * @brief predict the best S4FIFO configuration for the workload summarized
 * by `fv`.
 *
 * Mirrors the guard used by the model this was distilled from: a prediction
 * is only attempted once at least 10000 requests and 100 hits have been
 * observed in `fv` (an all-zero/too-small feature vector is not a
 * meaningful classifier input).
 *
 * @param fv observed features (see S4FIFO_features.h)
 * @param out populated with the predicted configuration iff this returns
 * true
 * @return true if a prediction was made (out is valid), false if the
 * guard wasn't met or the model produced an out-of-range class (out is
 * untouched - callers should keep their current configuration)
 */
bool s4fifo_predict(const S4FIFO_feature_vector_t *fv, S4FIFOConfigEntry *out);

/**
 * @brief same as s4fifo_predict(), except a non-empty `model_path` is
 * loaded first (see S4FIFO_model_real.h) and used in place of the
 * compiled-in default model. Falls back to the compiled-in default if
 * `model_path` is NULL/empty, or if loading it fails (logged once via
 * WARN, not fatal - auto-tune degrades to the default model rather than
 * disabling itself).
 */
bool s4fifo_predict_auto(const S4FIFO_feature_vector_t *fv,
                         const char *model_path, S4FIFOConfigEntry *out);

/**
 * @brief the 20 candidate configurations every model here chooses among -
 * exposed so S4FIFO_model_real.c doesn't need its own copy. A model's
 * output is an index into this table, so the two must stay in lockstep.
 */
const S4FIFOConfigEntry *s4fifo_get_config_table(void);

/**
 * @brief expose the real model's exact 76-feature input (the original 73 +
 * 3 within-window trend features, see S4FIFO_feature_vector_t), for
 * gathering (feature, best-of-20-config) training data at the same point in
 * a trace's lifecycle a real deployment would predict from - reuses the
 * same guard and feature derivation as s4fifo_predict_auto(), so training
 * and inference can never see differently-computed features.
 *
 * @param out76 populated iff this returns true
 * @return true if there's enough data collected in `fv` for `out76` to be
 * meaningful
 */
bool s4fifo_dump_features76(const S4FIFO_feature_vector_t *fv,
                            double *out76 /* [76] */);

#ifdef __cplusplus
}
#endif

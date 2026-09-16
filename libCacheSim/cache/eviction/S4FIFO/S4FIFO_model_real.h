//
// S4FIFO_model_real.h - evaluator for S4FIFO's learned control-plane
// models, in the compact `.s4m` binary format (see
// scripts/s4fifo_export_model.py).
//
// Two sources feed the same evaluator:
//   - the compiled-in default (model.h, transcribed from a
//     .s4m by scripts/s4fifo_embed_model.py) - used whenever no
//     `model-path=` is given, so `auto-tune=1` needs no setup at all;
//   - a `model-path=/path/to/model.s4m` override, for models too large to
//     vendor. Loaded once, lazily, shared read-only across every cache
//     instance in the process.
//

#pragma once

#include <stdbool.h>

#include "S4FIFO_predictor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief load the model at `path` if it hasn't been loaded yet (a no-op,
 * returning true, if a model - any model - is already loaded: this
 * simulator only ever runs with one model-path per process).
 *
 * Thread-safe; safe to call from every cache instance's init.
 *
 * @return true if a model is loaded and ready (whether by this call or an
 * earlier one), false if `path` couldn't be loaded (bad path/format).
 */
bool s4fifo_real_model_load(const char *path);

/**
 * @brief predict using the model loaded by s4fifo_real_model_load(), or -
 * if none was ever loaded successfully - the compiled-in default model.
 * Always has a model to work with; never requires a prior load.
 *
 * @param input the feature vector (see s4fifo_prepare_model_input76 - a
 * model exported before the 3 trailing trend features existed only ever
 * references indices < 73, so this same buffer works for either)
 * @param out populated with the predicted configuration iff this returns
 * true
 */
bool s4fifo_real_model_predict(const double *input, S4FIFOConfigEntry *out);

#ifdef __cplusplus
}
#endif

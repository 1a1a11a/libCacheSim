#pragma once

// Per-block prefill cost for LLM prefix-cache traces.
//
// Cost is a property of the model and the hardware, not of the trace, so
// traces store none. It is derived at simulation time from the block's 0-based
// position within its request, the only compute signal a trace carries. A
// model is given as a string:
//
//   linear:A,B  cost = A + B * position. A transformer's prefill FLOPs for a
//               block have this form: the linear layers cost the same for
//               every block, and attention grows with the context before it.
//   table:FILE  measured costs, one integer per line, line i for position i;
//               positions past the last line cost what the last line does.
//               Blank lines and lines starting with '#' are skipped.
//   uniform                 = linear:1,0, so compute saving == block hit ratio
//   position                = linear:1,1, the blocks from the root to this one
//   qwen3coder30b_blksz_16  = linear:865,2, measured on Qwen3-Coder-30B at
//                             16-token blocks
//
// Costs are integers from 1 to INT32_MAX, the range of cache_obj_t's cost:
// scale coefficients up instead of using fractions.
//
// Shared by the lcp reader (cachesim's `-t cost-model=`) and prefixsim
// (`--cost-model`), so the two cannot disagree.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLM_COST_MODEL_CHOICES \
  "uniform | position | qwen3coder30b_blksz_16 | linear:A,B | table:FILE"

typedef struct {
  int64_t a, b;   /**< cost = a + b * position, when n_table is 0 */
  int64_t *table; /**< cost by position, when n_table > 0 */
  int64_t n_table;
} llm_cost_model_t;

/** Every block costs 1. Owns no memory. */
static inline llm_cost_model_t llm_cost_model_uniform(void) {
  llm_cost_model_t model = {1, 0, NULL, 0};
  return model;
}

/** Parse `spec` (NULL means uniform). On failure, returns false with a message
 *  in `err` and leaves *out owning nothing. A table is read from its file now;
 *  release it with llm_cost_model_free(). */
bool llm_cost_model_parse(const char *spec, llm_cost_model_t *out, char *err,
                          size_t err_size);

void llm_cost_model_free(llm_cost_model_t *model);

static inline int64_t llm_block_cost(const llm_cost_model_t *model,
                                     int64_t position) {
  if (model->n_table > 0) {
    const int64_t last = model->n_table - 1;
    return model->table[position < last ? position : last];
  }
  return model->a + model->b * position;
}

/** The largest cost among positions 0 .. n_positions - 1. */
int64_t llm_cost_model_max(const llm_cost_model_t *model, int64_t n_positions);

/** Whether every position costs the same. */
bool llm_cost_model_is_uniform(const llm_cost_model_t *model);

#ifdef __cplusplus
}
#endif

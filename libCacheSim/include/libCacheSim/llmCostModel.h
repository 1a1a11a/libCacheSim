#pragma once

// Per-block prefill cost for LLM prefix-cache traces.
//
// Cost is a property of the model, not of the trace, so traces store none and
// it is derived at simulation time from the block's 0-based position within its
// request -- the only compute signal a trace carries. The constants belong to
// one model on one class of hardware; add an enumerator per profiled model
// rather than generalising the shape.
//
// Shared by the lcp reader (cachesim's `-t cost-model=`) and prefixsim
// (`--cost-model`), so the two cannot disagree.

#include <stdbool.h>
#include <stdint.h>
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  /** every block costs 1: compute saving ratio == block hit ratio */
  LLM_COST_UNIFORM = 0,
  /** position + 1: blocks that must be prefilled from the root to rebuild
   *  this one */
  LLM_COST_POSITION = 1,
  /** 865 + 2*position: measured per-block prefill cost of Qwen3-Coder-30B at
   *  16-token blocks -- a fixed part (projections, MLP) plus a part growing
   *  with the context attended to. Narrow range: position 1 vs 100 is 865 vs
   *  1065, so cost-aware policies rank close to cost-blind ones under it. */
  LLM_COST_QWEN3CODER30B_BLK16 = 2,
} llm_cost_model_e;

static inline int64_t llm_block_cost(llm_cost_model_e model, int64_t position) {
  switch (model) {
    case LLM_COST_UNIFORM:
      return 1;
    case LLM_COST_POSITION:
      return position + 1;
    case LLM_COST_QWEN3CODER30B_BLK16:
      return 865 + 2 * position;
  }
  return 1;
}

static inline bool llm_cost_model_parse(const char *name,
                                        llm_cost_model_e *out) {
  if (strcasecmp(name, "uniform") == 0) {
    *out = LLM_COST_UNIFORM;
  } else if (strcasecmp(name, "position") == 0) {
    *out = LLM_COST_POSITION;
  } else if (strcasecmp(name, "qwen3coder30b_blksz_16") == 0) {
    *out = LLM_COST_QWEN3CODER30B_BLK16;
  } else {
    return false;
  }
  return true;
}

static inline const char *llm_cost_model_name(llm_cost_model_e model) {
  switch (model) {
    case LLM_COST_UNIFORM:
      return "uniform";
    case LLM_COST_POSITION:
      return "position";
    case LLM_COST_QWEN3CODER30B_BLK16:
      return "qwen3coder30b_blksz_16";
  }
  return "?";
}

#ifdef __cplusplus
}
#endif

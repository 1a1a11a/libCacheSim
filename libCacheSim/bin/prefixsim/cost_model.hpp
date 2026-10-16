// Per-block compute cost.
//
// The compute saving ratio is cost-weighted, so the cost model is what decides
// what "saving" means. Every model is a function of the block's 0-based
// position within its request, the only compute signal a trace carries.
//
// The models themselves live in libCacheSim/llmCostModel.h, shared with the
// lcp reader so cachesim and prefixsim cannot disagree. This header is the
// C++ face of it.

#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include "libCacheSim/llmCostModel.h"
}

namespace prefixsim {

enum class CostModel {
  /// Every block costs 1, so the compute saving ratio equals the block hit
  /// ratio. Use this when you want the two numbers to be directly comparable.
  kUniform = LLM_COST_UNIFORM,
  /// cost = position + 1: the number of blocks that would have to be prefilled
  /// from the root to reconstruct this one.
  kPosition = LLM_COST_POSITION,
  /// cost = 865 + 2*position: the measured per-block prefill cost of
  /// Qwen3-Coder-30B at a 16-token block size. Matches the compute-intensity
  /// model in the vLLM prototype's logs (idx=0 -> 865, idx=100 -> 1065) and
  /// compute_intensity_transform() in the evaluate/ harness.
  ///
  /// Mind the dynamic range: position 1 vs 100 is 865 vs 1065, a factor of
  /// 1.23, against kPosition's factor of 100. Under this model a cost-aware
  /// policy ranks almost like a cost-blind one -- a property of the hardware,
  /// not a bug, and the first thing to check when compute-awareness looks like
  /// it is not paying.
  kQwen3Coder30bBlk16 = LLM_COST_QWEN3CODER30B_BLK16,
};

inline double block_cost(CostModel model, int64_t position) {
  return static_cast<double>(
      llm_block_cost(static_cast<llm_cost_model_e>(model), position));
}

inline bool parse_cost_model(const std::string &name, CostModel &out) {
  llm_cost_model_e m;
  if (!llm_cost_model_parse(name.c_str(), &m)) return false;
  out = static_cast<CostModel>(m);
  return true;
}

inline const char *cost_model_name(CostModel model) {
  return llm_cost_model_name(static_cast<llm_cost_model_e>(model));
}

/// True when a block's cost depends on where it sits in the prefix, which means
/// two different prefixes reaching the same *raw* block id disagree about cost.
inline bool cost_is_position_dependent(CostModel model) {
  return model != CostModel::kUniform;
}

}  // namespace prefixsim

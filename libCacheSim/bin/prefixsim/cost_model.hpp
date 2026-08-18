// Per-block compute cost.
//
// The compute saving ratio is cost-weighted, so the cost model is what decides
// what "saving" means. Every model here is a function of the block's 0-based
// position within its request, which is the only signal the qwen trace carries.

#pragma once

#include <cstdint>
#include <string>

namespace prefixsim {

enum class CostModel {
  /// Every block costs 1, so the compute saving ratio equals the block hit
  /// ratio. Use this when you want the two numbers to be directly comparable.
  kUniform,
  /// cost = position + 1. Matches the `compute` field that
  /// llm_cachesim/scripts/trace_processing/llm_qwen.py writes into .lcsllm:
  /// the number of blocks that would have to be prefilled from the root to
  /// reconstruct this one.
  kPosition,
  /// cost = 863 + 2*(position+1), reproducing compute_intensity_transform() in
  /// the evaluate/ harness. Provided for comparison only: the large constant
  /// compresses the dynamic range so far (position 1 vs 100 is 865 vs 1063)
  /// that cost-aware policies rank almost identically to cost-blind ones.
  kAffine,
};

inline double block_cost(CostModel model, int64_t position) {
  const double pos1 = static_cast<double>(position + 1);
  switch (model) {
    case CostModel::kUniform:
      return 1.0;
    case CostModel::kPosition:
      return pos1;
    case CostModel::kAffine:
      return 863.0 + 2.0 * pos1;
  }
  return 1.0;
}

inline bool parse_cost_model(const std::string &name, CostModel &out) {
  if (name == "uniform") {
    out = CostModel::kUniform;
  } else if (name == "position") {
    out = CostModel::kPosition;
  } else if (name == "affine") {
    out = CostModel::kAffine;
  } else {
    return false;
  }
  return true;
}

inline const char *cost_model_name(CostModel model) {
  switch (model) {
    case CostModel::kUniform:
      return "uniform";
    case CostModel::kPosition:
      return "position";
    case CostModel::kAffine:
      return "affine";
  }
  return "?";
}

/// True when a block's cost depends on where it sits in the prefix, which means
/// two different prefixes reaching the same *raw* block id disagree about cost.
inline bool cost_is_position_dependent(CostModel model) {
  return model != CostModel::kUniform;
}

}  // namespace prefixsim

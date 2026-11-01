// Per-block compute cost.
//
// The compute saving ratio is cost-weighted, so the cost model is what decides
// what "saving" means. Every model is a function of the block's 0-based
// position within its request, the only compute signal a trace carries.
//
// The models themselves live in libCacheSim/llmCostModel.h, shared with the
// lcp reader so cachesim and prefixsim cannot disagree; see there for the
// model strings (uniform, position, qwen3coder30b_blksz_16, linear:A,B,
// table:FILE). This header is the C++ face of it.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

extern "C" {
#include "libCacheSim/llmCostModel.h"
}

namespace prefixsim {

/// A parsed cost model. Copies share one immutable model, so it is cheap to
/// hand to every simulation.
class CostModel {
 public:
  /// Uniform: every block costs 1, so the compute saving ratio equals the
  /// block hit ratio.
  CostModel() : name_("uniform") {}

  /// Parse `spec` into `out`; false with `error` set when it is not a model.
  static bool parse(const std::string &spec, CostModel &out,
                    std::string &error) {
    auto model = std::shared_ptr<llm_cost_model_t>(new llm_cost_model_t,
                                                   [](llm_cost_model_t *m) {
                                                     llm_cost_model_free(m);
                                                     delete m;
                                                   });
    char err[1024];
    if (!llm_cost_model_parse(spec.c_str(), model.get(), err, sizeof(err))) {
      error = err;
      return false;
    }
    out.name_ = spec;
    out.model_ = std::move(model);
    return true;
  }

  int64_t cost(int64_t position) const {
    return model_ ? llm_block_cost(model_.get(), position) : 1;
  }

  /// The string the model was parsed from, for reports.
  const std::string &name() const { return name_; }

  /// True when a block's cost depends on where it sits in the prefix, which
  /// means two different prefixes reaching the same *raw* block id disagree
  /// about cost.
  bool position_dependent() const {
    return model_ && !llm_cost_model_is_uniform(model_.get());
  }

 private:
  std::string name_;
  std::shared_ptr<llm_cost_model_t> model_;  ///< null means uniform
};

}  // namespace prefixsim

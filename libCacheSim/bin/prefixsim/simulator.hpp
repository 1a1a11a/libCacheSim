// The three-phase prefix-cache simulator.
//
// See README.md for the full rationale. Each request is served as:
//   1. match     -- read-only probe of every block; produces all statistics
//   2. allocate  -- evict until there is room for the *missing* blocks, without
//                   counting victims that this same request is about to reuse
//   3. access    -- replay the request through the eviction algorithm, deepest
//                   block first
//
// Phase 2 is the piece libCacheSim does not provide on its own: an LLM request
// needs all of its blocks resident simultaneously, and freeing space naively
// evicts blocks the arriving request was about to reuse.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

#include "cost_model.hpp"
#include "trace.hpp"

extern "C" {
#include "libCacheSim/cache.h"
#include "libCacheSim/prefetchAlgo.h"
}

namespace prefixsim {

struct Stats {
  // ---- request level ----
  int64_t n_requests = 0;
  /// Requests with more distinct blocks than the entire cache. They can never
  /// be fully resident, so they are skipped and excluded from every ratio.
  int64_t n_requests_skipped = 0;

  // ---- block level: the headline numbers ----
  int64_t n_blocks = 0;      ///< Block accesses across all served requests.
  int64_t n_block_hits = 0;  ///< Blocks already resident when the request arrived.
  double total_cost = 0.0;   ///< Sum of block_cost() over all blocks.
  double saved_cost = 0.0;   ///< Sum of block_cost() over hit blocks.

  // ---- diagnostics ----
  int64_t n_evictions = 0;       ///< Objects evicted during phase 2.
  int64_t n_self_evictions = 0;  ///< ...that the arriving request itself needed.
  /// Evictions triggered from inside phase 3. Must stay 0: phase 2 reserves
  /// exactly enough room. Non-zero means the allocation arithmetic and the
  /// eviction algorithm disagree, and the run is not trustworthy.
  int64_t n_unexpected_evictions = 0;
  /// Allocation rounds where the algorithm evicted without reporting a victim
  /// (it evicts inside sub-caches whose prefetcher is not ours), so the deficit
  /// was recomputed by re-probing instead. Diagnostic only: n_self_evictions is
  /// still exact, because the same re-probe detects Alpha's blocks going away.
  int64_t n_unobserved_eviction_rounds = 0;

  double block_hit_ratio() const {
    return n_blocks ? static_cast<double>(n_block_hits) / static_cast<double>(n_blocks) : 0.0;
  }
  double compute_saving_ratio() const {
    return total_cost > 0.0 ? saved_cost / total_cost : 0.0;
  }
};

struct SimulatorConfig {
  int64_t cache_size_blocks = 0;
  CostModel cost_model = CostModel::kUniform;
  /// After phase 3, re-probe every block of the request and fail if any is
  /// missing. Catches eviction algorithms whose resident set is not visible in
  /// the main hash table (the S3FIFO family, for instance), which would
  /// silently invalidate the phase-1 match.
  bool verify = true;
  /// Make the arriving request's own blocks unevictable while phase 2 frees
  /// space for it, the way a real engine ref-counts the blocks of a running
  /// request. Without it the algorithm may evict a block this request already
  /// holds; the block still counts as a hit (phase 1 matched it before any
  /// eviction), but it re-enters through insert() and so loses its place in
  /// the policy's structure -- an S3FIFO main-queue block, for instance, comes
  /// back in the small queue because a main-queue eviction writes no ghost
  /// entry. Off reproduces the pre-2026-09 numbers.
  bool pin_request_blocks = true;
  /// Phase 3 replay order. True (the default) touches the deepest block
  /// first, so recency-ordered algorithms leave the prefix root at the MRU end
  /// and eviction eats the prefix from the deep end inward (see README.md
  /// "Reverse-order replay"). False replays in prefix order, which leaves the
  /// root nearest the LRU end, so eviction starts from the beginning.
  bool replay_deepest_first = true;
  /// When set, append one line per served request describing that request's
  /// holes -- the contiguous runs of blocks it had to recompute. Empty
  /// disables it. See Simulator::dump_holes.
  std::string dump_holes_path;
};

/// Whether phase-2 pinning (SimulatorConfig::pin_request_blocks) is sound for
/// this algorithm. False for an unknown name, and for the algorithms that
/// assume an eviction removes the exact object they last looked at.
bool algorithm_supports_pinning(const std::string &algorithm);

/// Runs one eviction algorithm over an in-memory trace.
class Simulator {
 public:
  Simulator(cache_t *cache, const SimulatorConfig &config);
  ~Simulator();

  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;

  /// Serve every request in order. Returns false and fills `error` if an
  /// invariant broke.
  bool run(const std::vector<Request> &requests, std::string &error);

  const Stats &stats() const { return stats_; }

 private:
  bool serve(const Request &request, std::string &error);

  /// Phase 1: is this block resident right now? Probes the hash table directly
  /// so the eviction algorithm never registers the probe as an access.
  bool probe(obj_id_t id) const;

  /// Phase 2: evict until `needed` blocks that this request does *not* want
  /// have been freed.
  bool allocate(const Request &request, int64_t needed, std::string &error);

  /// Phase 3: replay the request, deepest block first.
  void access(const Request &request);

  /// Point req_buf_ at block `i` of `request`.
  void fill_request(const Request &request, size_t i);

  /// Write this request's holes, in the format analyze_holes.py parses.
  void dump_holes(const Request &request);

  cache_t *cache_ = nullptr;
  SimulatorConfig config_;
  Stats stats_;

  request_t *req_buf_ = nullptr;
  prefetcher_t *recorder_ = nullptr;
  FILE *holes_file_ = nullptr;

  // Scratch reused across requests so the hot loop does not allocate.
  std::vector<char> resident_;
  std::unordered_set<obj_id_t> alpha_;
  std::unordered_set<obj_id_t> alpha_resident_;
  std::vector<obj_id_t> victims_;
};

/// Create a cache by algorithm name (case-insensitive). Returns nullptr for
/// unknown names. `cache_size_blocks` is in blocks: every object has size 1.
cache_t *create_cache_by_name(const std::string &algorithm, int64_t cache_size_blocks,
                              const char *cache_specific_params = nullptr,
                              int64_t n_total_req = 0);

/// Algorithms whose entire resident set lives in the main hash table, i.e. the
/// ones the phase-1 probe is valid for.
const std::vector<std::string> &supported_algorithms();

}  // namespace prefixsim

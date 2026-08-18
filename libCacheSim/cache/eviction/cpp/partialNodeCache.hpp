// Base class for sample-based partial-node eviction algorithms.
//
// The shape mirrors RadixTreeFreeBlockManager in the vLLM prototype: the base
// owns the prefix tree, the sampling, the candidate selection and all of the
// cache_t bookkeeping; a derived algorithm supplies only a per-block score.
//
// One eviction round:
//   1. sample n_sample NODES uniformly (not blocks -- that is the difference
//      from RandomCompute and friends, which sample the flat hash table)
//   2. take each sampled node's eviction candidate: its deepest resident block
//      by default, see `evict_from_tail`
//   3. score the candidates and evict the single worst one
//
// Evicting one block out of a multi-block node is what makes this "partial
// node": the rest of the node stays cached.
//
// To add a variant, derive from PartialNodeCache, override score(), and write
// a five-line _init that hands the instance to partial_node_cache_init().
// See PartialNodeRandomCompute.cpp.

#pragma once

#include <string>

#include "prefixRadixTree.hpp"

extern "C" {
#include "libCacheSim/cache.h"
}

namespace eviction {

class PartialNodeCache {
 public:
  virtual ~PartialNodeCache() = default;

  /**
   * Score one candidate block. LOWER scores are evicted first, matching the
   * convention in RandomCompute and the other sampling algorithms here.
   *
   * `obj` is guaranteed to be in the cache. Everything a score can depend on
   * lives on the object (cost, misc.freq, Random.last_access_vtime,
   * misc.next_access_vtime) or on the cache (n_req).
   */
  virtual double score(const cache_t *cache, const cache_obj_t *obj) const = 0;

  /**
   * Optional: called once per request with the whole block path, before those
   * blocks are accessed. The base class has already inserted the path into the
   * tree by the time this runs; override only to keep extra per-request state.
   */
  virtual void on_record_request(const obj_id_t * /*ids*/, int64_t /*n*/) {}

  /// How many blocks one eviction round takes out of the winning node.
  ///
  /// Both modes sample n_sample nodes, score each by its canonical block (see
  /// node_chunk_score, which follows vLLM's rule), and take the chunk from the
  /// best-scoring node. They differ in the cap, and in which canonical rule
  /// applies:
  ///   kDrain  -- uncapped: the winning node gives up the entire remaining
  ///              deficit if it can. One sample per round, cheapest, but it
  ///              commits to a single node for a potentially large batch.
  ///   kMicro  -- capped at micro_batch blocks, then re-sample and re-score.
  ///              More selective for the same reason drain is cheaper.
  /// Named after EVICTION_MODE in the vLLM prototype.
  enum class Mode { kDrain, kMicro };

  PrefixRadixTree tree;

  Mode mode = Mode::kDrain;
  /// Blocks per round in kMicro. vLLM's MICRO_BATCH_SIZE.
  int64_t micro_batch = 64;

  /// Nodes drawn per eviction round. vLLM calls this ASSOCIATIVITY.
  int n_sample = 32;
  /// Which end of a node to evict from.
  ///
  /// Head (shallowest resident block) is the default, matching vLLM's
  /// leftmost-evictable choice. Tail looks more natural for a prefix cache --
  /// drop the private end, keep the shared root -- but it interacts badly with
  /// a position-based cost model: the deepest block of a node is also the most
  /// expensive one, so restricting candidates to tails forces the policy to
  /// evict exactly what its own score wants to keep. Measured on qwen_traceA at
  /// 8k blocks with the position cost model, tail scores 0.0129 compute saving
  /// against head's 0.0580.
  bool evict_from_tail = false;
};

/**
 * Build a cache_t around `impl`, which the returned cache takes ownership of.
 * Installs the shared vtable, including record_request.
 *
 * Recognised cache_specific_params: n-sample=<int>, evict-from=<tail|head>,
 * eviction-mode=<drain|micro>, micro-batch=<int>.
 */
cache_t *partial_node_cache_init(const char *cache_name,
                                 common_cache_params_t ccache_params,
                                 const char *cache_specific_params,
                                 PartialNodeCache *impl);

}  // namespace eviction

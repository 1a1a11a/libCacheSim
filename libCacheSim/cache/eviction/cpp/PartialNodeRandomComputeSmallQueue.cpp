// Partial-node RandomCompute behind an S3FIFO-style admission queue.
//
// Same score as PartialNodeRandomCompute (cost / recency), same partial-node
// eviction on the prefix tree -- but only for the main set. In front of it sits
// a small FIFO queue that admits every newly-seen block and discards it again
// unless it is referenced move_to_main_threshold times, with a ghost list so a
// block that comes straight back skips the queue on its second chance.
//
// The two tiers are deliberately asymmetric. The main set is where the
// expensive, structure-aware decision belongs: sample prefix-tree nodes, score
// them, evict part of one. The small queue is a plain FIFO with no sampling and
// no tree involvement, because its only job is to keep one-hit wonders from
// ever displacing anything in the main set -- and doing that cheaply is the
// whole point.
//
// Counterpart of RandomSmallQueueFreeBlockManager in the vLLM prototype, whose
// admission rules (10% small, 90% ghost, promote at 2 accesses) these defaults
// follow. Everything except the score and those defaults lives in
// PartialNodeCache.

#include "partialNodeCache.hpp"

namespace eviction {

class PartialNodeRandomComputeSmallQueue : public PartialNodeCache {
 public:
  PartialNodeRandomComputeSmallQueue() {
    small_size_ratio = 0.10;
    ghost_size_ratio = 0.90;
    move_to_main_threshold = 2;
  }

  /// cost / recency, identical to PartialNodeRandomCompute. Frequency is absent
  /// on purpose: the small queue already filters on it, so paying for it twice
  /// would double-count the same signal.
  double score(const cache_t *cache, const cache_obj_t *obj) const override {
    const int64_t age = cache->n_req - obj->Random.last_access_vtime;
    const int64_t recency = age > 1 ? age : 1;
    return static_cast<double>(obj->cost) / static_cast<double>(recency);
  }
};

}  // namespace eviction

#ifdef __cplusplus
extern "C" {
#endif

cache_t *PartialNodeRandomComputeSmallQueue_init(
    const common_cache_params_t ccache_params,
    const char *cache_specific_params) {
  return eviction::partial_node_cache_init(
      "PartialNodeRandomComputeSmallQueue", ccache_params,
      cache_specific_params,
      new eviction::PartialNodeRandomComputeSmallQueue());
}

#ifdef __cplusplus
}
#endif

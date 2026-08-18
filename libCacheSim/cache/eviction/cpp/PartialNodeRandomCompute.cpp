// Partial-node RandomCompute.
//
// RandomCompute's score applied to node-sampled candidates: instead of drawing
// n_sample blocks uniformly from the hash table, draw n_sample prefix-tree
// NODES and consider only each node's deepest resident block. Evicting that
// one block leaves the rest of the node cached, so a long shared prefix is
// eaten from its private end inward instead of being punched full of holes.
//
// Everything except the score lives in PartialNodeCache.

#include "partialNodeCache.hpp"

namespace eviction {

class PartialNodeRandomCompute : public PartialNodeCache {
 public:
  /// cost / recency -- identical to _rc_cost() in RandomCompute.c. Frequency
  /// is deliberately absent: it carries no signal on the LLM traces.
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

cache_t *PartialNodeRandomCompute_init(const common_cache_params_t ccache_params,
                                       const char *cache_specific_params) {
  return eviction::partial_node_cache_init("PartialNodeRandomCompute",
                                           ccache_params, cache_specific_params,
                                           new eviction::PartialNodeRandomCompute());
}

#ifdef __cplusplus
}
#endif

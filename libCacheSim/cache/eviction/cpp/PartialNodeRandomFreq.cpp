// Partial-node RandomCompute with the frequency term kept.
//
// Exists to reproduce RandomFreeBlockManager ("Random") from the vLLM
// prototype, whose score is access_count / recency * compute_intensity. The
// production variant, PartialNodeRandomCompute, drops the frequency factor;
// this one is the controlled comparison that isolates what that factor is
// worth. Keep them side by side rather than making frequency a parameter, so
// each name means exactly one algorithm.
//
// misc.freq counts accesses after the insert (cache_insert_base zeroes it,
// cache_find_base increments it), so misc.freq + 1 is vLLM's access_count,
// which starts at 1 on first access. cache->n_req ticks once per block access,
// matching vLLM's current_time.

#include "partialNodeCache.hpp"

namespace eviction {

class PartialNodeRandomFreq : public PartialNodeCache {
 public:
  double score(const cache_t *cache, const cache_obj_t *obj) const override {
    const int64_t age = cache->n_req - obj->Random.last_access_vtime;
    const int64_t recency = age > 1 ? age : 1;
    return static_cast<double>(obj->misc.freq + 1) *
           static_cast<double>(obj->cost) / static_cast<double>(recency);
  }
};

}  // namespace eviction

#ifdef __cplusplus
extern "C" {
#endif

cache_t *PartialNodeRandomFreq_init(const common_cache_params_t ccache_params,
                                    const char *cache_specific_params) {
  return eviction::partial_node_cache_init("PartialNodeRandomFreq", ccache_params,
                                           cache_specific_params,
                                           new eviction::PartialNodeRandomFreq());
}

#ifdef __cplusplus
}
#endif

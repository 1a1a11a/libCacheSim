/* WorkloadAware: the eviction policy of "KVCache Cache in the Wild"
 * (arXiv:2506.02634, USENIX ATC'25), §4.2 / Fig. 23-24.
 *
 * Evicts the block with the lexicographically smallest priority
 *
 *     Priority = ( ReuseProb_w(t, life) ,  -Offset )
 *
 * where
 *
 *   - ReuseProb_w(t, life) = F_w(t + life) - F_w(t) is the probability the block
 *     is reused within the next `life` given it has already been idle for `t`,
 *     read off the fitted reuse-time CDF of the block's workload class w. The
 *     paper fits an exponential, F_w(x) = 1 - exp(-lambda_w x), so
 *
 *         ReuseProb_w(t, life) = exp(-lambda_w t) * (1 - exp(-lambda_w life))
 *
 *     The second factor is the paper's key device: it *regulates* the reuse
 *     probability by the lifespan, so a class with a long reuse tail is not held
 *     at high priority indefinitely -- which would contradict how short a KV
 *     block's life actually is. Without it the two classes could not be compared
 *     at all, since the factor is the only thing that is class-specific once t
 *     is fixed.
 *
 *   - Offset is the block's position in the prefix. Because the tuple is
 *     compared lexicographically and the second element is negated, ties in
 *     reuse probability evict the *deeper* block, retaining shared prefix heads
 *     (the spatial locality of §3.4). Fig. 24 line 7 spells out the same rule as
 *     "b.offset > ChosenBlock.offset".
 *
 * Frequency is deliberately absent (§4.2): KV blocks are ephemeral, so how often
 * a block was used in the past says little about whether it will be used again.
 *
 * ------------------------------------------------------------------ complexity
 * The naive form scores every resident block per eviction. The paper's
 * optimization (§4.2 "Performance optimization") is that within one class the
 * blocks are already ordered by last access, and ReuseProb is monotone
 * decreasing in t, so the class's LRU head is the only candidate that class can
 * ever offer. Eviction therefore compares one candidate per class: O(W) for W
 * classes instead of O(N) for N blocks.
 *
 * That is what this implements -- one intrusive LRU list per class, plus a heap
 * over the class heads. The heap is rebuilt whenever the logical clock has
 * advanced, because ReuseProb depends on `now` and a clock tick reorders the
 * heads; within one eviction batch the clock is fixed, so the heap is instead
 * maintained incrementally as heads are consumed.
 *
 * -------------------------------------------------------------------- fidelity
 * Two things the paper profiles offline are fit online here, because a simulator
 * has no background sampler:
 *
 *   - lambda_w = 1/mean_gap_w, from an EWMA of the reuse gaps observed in class
 *     w. The paper samples an hour of traffic in the background and refits
 *     periodically; an EWMA is the streaming form of the same estimate.
 *   - life, the global expected lifespan, from an EWMA of all observed gaps.
 *
 * The clock is a *block counter*, incremented once per recorded block access,
 * not wall-clock seconds (the paper's Fig. 24 reads `time.time()`). This matches
 * the vLLM prototype that these numbers are compared against. It matters less
 * here than it does for AsymCache, because what separates this policy from LRU
 * is the per-class partitioning and the offset tiebreak rather than the shape of
 * the decay -- but see the note in AsymCache.cpp for what a block-count clock
 * does to an exponential whose lifespan is calibrated in seconds.
 */

#include <cmath>
#include <cstdint>
#include <list>
#include <queue>
#include <unordered_map>
#include <vector>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/evictionAlgo.h"

namespace eviction {
namespace {

/// EWMA responsiveness for the reuse-gap fits.
constexpr double kEwmaAlpha = 0.05;
/// Prior mean gap for a class no reuse has been observed in yet. Large, so an
/// unseen class starts out near-LRU rather than being thrown away immediately.
constexpr double kDefaultMeanGap = 1.0e6;

struct BlockMeta {
  uint64_t category = 0;
  int64_t last_access = 0;
  int64_t offset = 0;
  /// False between record_request (which learns a block's class and offset) and
  /// the insert that actually admits it. Only resident blocks live in a class's
  /// LRU list, so only they can be chosen as victims -- evicting a block that is
  /// merely *known* would hand the caller an id the hash table has never seen.
  bool resident = false;
  /// Position in the class's LRU list. Valid only while `resident`.
  std::list<obj_id_t>::iterator slot;
};

struct CategoryQueue {
  /// front = least recently accessed = this class's only eviction candidate.
  std::list<obj_id_t> lru;
  double mean_gap = kDefaultMeanGap;
};

/// One entry of the class-head heap. `head` is the candidate this entry was
/// built for; it is re-derived on pop, so a stale entry is detected rather than
/// trusted.
struct HeadEntry {
  double reuse_prob;
  int64_t neg_offset;
  uint64_t seq;
  uint64_t category;
  obj_id_t head;

  /// std::priority_queue is a max-heap, so `operator<` is inverted to make the
  /// smallest priority tuple surface first. Ordering is exactly the paper's
  /// lexicographic (ReuseProb, -Offset), with a sequence number last so the
  /// order is total and the run is reproducible.
  bool operator<(const HeadEntry &rhs) const {
    if (reuse_prob != rhs.reuse_prob) return reuse_prob > rhs.reuse_prob;
    if (neg_offset != rhs.neg_offset) return neg_offset > rhs.neg_offset;
    return seq > rhs.seq;
  }
};

class WorkloadAware {
 public:
  /// Logical clock: one tick per recorded block access.
  int64_t now = 0;
  /// Global expected lifespan of a KV block, EWMA of every observed reuse gap.
  double life = kDefaultMeanGap;
  /// The class of the request currently being served, from set_request_ctx.
  uint64_t cur_category = 0;
  /// Set once record_request fires. Until then nothing advances the clock, so
  /// every reuse probability would be equal and the policy would decay to
  /// insertion order. A caller that drives the cache one object at a time gets a
  /// per-access clock instead, which is the same thing record_request would have
  /// produced for single-block requests.
  bool hooked = false;

  std::unordered_map<obj_id_t, BlockMeta> meta;
  std::unordered_map<uint64_t, CategoryQueue> queues;
  /// Categories in first-seen order. The head heap must be built in a
  /// deterministic order, because ReuseProb genuinely ties: exp(-lambda_w * t)
  /// is *exactly* 0.0 in double precision once t/mean_gap_w exceeds ~745, which
  /// on a trace of this length is routine for a class with tight reuse whose
  /// blocks have since gone cold. Several classes then present priority
  /// (0.0, -offset), and offsets tie too, so the decision falls through to the
  /// insertion sequence -- i.e. to whatever order the categories were visited
  /// in. Iterating `queues` directly would make that order a function of the
  /// hash table's layout, so the same trace could yield different results after
  /// an unrelated change. This keeps it first-seen, which is also the order the
  /// vLLM prototype gets for free from Python's insertion-ordered dict.
  std::vector<uint64_t> category_order;

  std::priority_queue<HeadEntry> head_heap;
  /// Clock value head_heap was built for; -1 forces a rebuild.
  int64_t heap_clock = -1;
  uint64_t seq = 0;

  /// The class's queue, creating it on first use. Every creation goes through
  /// here so `category_order` can never fall out of step with `queues`, which is
  /// what keeps heap rebuilds deterministic.
  CategoryQueue &queue_for(uint64_t category) {
    const size_t before = queues.size();
    CategoryQueue &q = queues[category];
    if (queues.size() != before) category_order.push_back(category);
    return q;
  }

  double mean_gap_of(uint64_t category) const {
    auto it = queues.find(category);
    return it == queues.end() ? kDefaultMeanGap : it->second.mean_gap;
  }

  /// ReuseProb_w(t, life) for a resident block, per Fig. 23.
  double reuse_prob(obj_id_t id) const {
    auto it = meta.find(id);
    if (it == meta.end()) return 0.0;
    const double mean_gap = mean_gap_of(it->second.category);
    const double lambda = mean_gap > 0.0 ? 1.0 / mean_gap : 0.0;
    const double t = static_cast<double>(now - it->second.last_access);
    return std::exp(-lambda * t) * (1.0 - std::exp(-lambda * life));
  }

  /// This class's eviction candidate, or 0-with-false when the class holds
  /// nothing resident. The list front is the least recently accessed block, and
  /// ReuseProb is monotone decreasing in idle time, so the front is the only
  /// candidate this class can ever offer (the paper's O(N) -> O(W) argument).
  bool category_head(uint64_t category, obj_id_t &out) const {
    auto it = queues.find(category);
    if (it == queues.end() || it->second.lru.empty()) return false;
    out = it->second.lru.front();
    return true;
  }

  /// Admit a resident block to the MRU end of its class's LRU list.
  void make_resident(obj_id_t id, BlockMeta &m) {
    CategoryQueue &q = queue_for(m.category);
    q.lru.push_back(id);
    m.slot = std::prev(q.lru.end());
    m.resident = true;
  }

  void push_head(uint64_t category, obj_id_t head) {
    auto it = meta.find(head);
    const int64_t offset = it == meta.end() ? 0 : it->second.offset;
    head_heap.push(HeadEntry{reuse_prob(head), -offset, seq++, category, head});
  }

  void rebuild_heap() {
    head_heap = std::priority_queue<HeadEntry>();
    for (const uint64_t category : category_order) {
      obj_id_t head = 0;
      if (category_head(category, head)) push_head(category, head);
    }
    heap_clock = now;
  }

  /// Drop a block from all bookkeeping. Called for both eviction and removal,
  /// matching the reference, which discards a victim's class, offset and
  /// last-access together: a block that comes back is a new block.
  void forget(obj_id_t id) {
    auto it = meta.find(id);
    if (it == meta.end()) return;
    if (it->second.resident) {
      auto q = queues.find(it->second.category);
      if (q != queues.end()) q->second.lru.erase(it->second.slot);
      // The per-class mean gap survives: it is a property of the workload, not
      // of whatever happens to be resident, so draining a small class must not
      // reset its fit back to the prior.
    }
    meta.erase(it);
  }

  /// Pick the victim. Returns false only when nothing resident is left.
  bool choose_victim(obj_id_t &out) {
    if (heap_clock != now) rebuild_heap();

    while (!head_heap.empty()) {
      const HeadEntry top = head_heap.top();
      head_heap.pop();

      obj_id_t cur = 0;
      if (!category_head(top.category, cur)) continue;  // class drained
      if (cur != top.head) {
        push_head(top.category, cur);  // stale entry: re-key the real head
        continue;
      }
      out = cur;
      return true;
    }

    // The heap is built from every non-empty class, so draining it means no
    // class has a resident block left. Scan once to be certain before telling
    // the caller there is nothing to evict, which it treats as fatal.
    for (const uint64_t category : category_order) {
      const auto it = queues.find(category);
      if (it != queues.end() && !it->second.lru.empty()) {
        out = it->second.lru.front();
        return true;
      }
    }
    return false;
  }

  /// Maintain the heap after a victim leaves: its class may still have a head.
  void repush_after_eviction(uint64_t category) {
    obj_id_t next = 0;
    if (category_head(category, next)) push_head(category, next);
  }
};

}  // namespace
}  // namespace eviction

using eviction::WorkloadAware;

#ifdef __cplusplus
extern "C" {
#endif

cache_t *WorkloadAware_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params);
static void WorkloadAware_free(cache_t *cache);
static bool WorkloadAware_get(cache_t *cache, const request_t *req);
static cache_obj_t *WorkloadAware_find(cache_t *cache, const request_t *req,
                                       bool update_cache);
static cache_obj_t *WorkloadAware_insert(cache_t *cache, const request_t *req);
static cache_obj_t *WorkloadAware_to_evict(cache_t *cache, const request_t *req);
static void WorkloadAware_evict(cache_t *cache, const request_t *req);
static bool WorkloadAware_remove(cache_t *cache, obj_id_t obj_id);
static void WorkloadAware_set_request_ctx(cache_t *cache,
                                          const cache_request_ctx_t *ctx);
static void WorkloadAware_record_request(cache_t *cache, const obj_id_t *ids,
                                         int64_t n);

static inline WorkloadAware *wa_of(const cache_t *cache) {
  return reinterpret_cast<WorkloadAware *>(cache->eviction_params);
}

cache_t *WorkloadAware_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("WorkloadAware", ccache_params, cache_specific_params);
  cache->eviction_params = reinterpret_cast<void *>(new WorkloadAware);

  cache->cache_init = WorkloadAware_init;
  cache->cache_free = WorkloadAware_free;
  cache->get = WorkloadAware_get;
  cache->find = WorkloadAware_find;
  cache->insert = WorkloadAware_insert;
  cache->evict = WorkloadAware_evict;
  cache->remove = WorkloadAware_remove;
  cache->to_evict = WorkloadAware_to_evict;
  cache->set_request_ctx = WorkloadAware_set_request_ctx;
  cache->record_request = WorkloadAware_record_request;

  // An empty string is not "no parameters given": simulate_at_multi_sizes()
  // re-inits the cache with cache->init_params, which cache_struct_init sets to
  // "" when none were passed. Rejecting that broke every multi-size run.
  if (cache_specific_params != nullptr && cache_specific_params[0] != '\0') {
    ERROR("WorkloadAware does not support any parameters, but got %s\n",
          cache_specific_params);
  }

  return cache;
}

static void WorkloadAware_free(cache_t *cache) {
  delete wa_of(cache);
  cache_struct_free(cache);
}

static bool WorkloadAware_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

static cache_obj_t *WorkloadAware_find(cache_t *cache, const request_t *req,
                                       const bool update_cache) {
  // Recency is maintained in record_request, which is the only place that knows
  // the request's block order. A find() must therefore not reorder anything, or
  // the phase-1 residency probe (which calls find with update_cache = false,
  // but also every ordinary hit) would perturb the policy.
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  WorkloadAware *wa = wa_of(cache);
  if (!wa->hooked && update_cache) {
    // No request structure available: this access *is* the clock tick.
    if (obj != nullptr) {
      auto it = wa->meta.find(req->obj_id);
      if (it != wa->meta.end()) {
        const int64_t gap = wa->now - it->second.last_access;
        eviction::CategoryQueue &q = wa->queue_for(it->second.category);
        if (gap > 0) {
          q.mean_gap = (1.0 - eviction::kEwmaAlpha) * q.mean_gap +
                       eviction::kEwmaAlpha * static_cast<double>(gap);
          wa->life = (1.0 - eviction::kEwmaAlpha) * wa->life +
                     eviction::kEwmaAlpha * static_cast<double>(gap);
        }
        it->second.last_access = wa->now;
        if (it->second.resident) q.lru.splice(q.lru.end(), q.lru, it->second.slot);
      }
    }
    ++wa->now;
    wa->heap_clock = -1;
  }

  return obj;
}

static cache_obj_t *WorkloadAware_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  if (obj == nullptr) return nullptr;

  WorkloadAware *wa = wa_of(cache);
  auto it = wa->meta.find(req->obj_id);

  if (it != wa->meta.end()) {
    // record_request already learned this block's class, offset and last-access;
    // admitting it only has to file it into the class's LRU list.
    if (!it->second.resident) wa->make_resident(req->obj_id, it->second);
    return obj;
  }

  // No record_request ran, which is the case when the caller drives the cache
  // one object at a time (plain cachesim, the test suite). Fall back to the
  // current request's class and an unknown offset, so the policy still functions
  // without the hook -- degraded to "one class, all offsets tie" rather than
  // broken.
  eviction::BlockMeta m;
  m.category = wa->cur_category;
  m.last_access = wa->now;
  m.offset = 0;
  wa->make_resident(req->obj_id, m);
  wa->meta.emplace(req->obj_id, m);

  return obj;
}

static cache_obj_t *WorkloadAware_to_evict(cache_t *cache, const request_t *req) {
  (void)req;
  obj_id_t victim = 0;
  if (!wa_of(cache)->choose_victim(victim)) return nullptr;
  return hashtable_find_obj_id(cache->hashtable, victim);
}

static void WorkloadAware_evict(cache_t *cache, const request_t *req) {
  (void)req;
  WorkloadAware *wa = wa_of(cache);

  obj_id_t victim = 0;
  if (!wa->choose_victim(victim)) {
    ERROR("WorkloadAware: asked to evict with an empty cache\n");
    return;
  }

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, victim);
  if (obj == nullptr) {
    // Bookkeeping drifted from the hash table. Drop the phantom and let the
    // caller retry rather than corrupting the accounting.
    wa->forget(victim);
    ERROR("WorkloadAware: victim %llu is not in the hash table\n",
          static_cast<unsigned long long>(victim));
    return;
  }

  const auto vit = wa->meta.find(victim);
  const uint64_t category = vit == wa->meta.end() ? 0 : vit->second.category;
  wa->forget(victim);
  cache_evict_base(cache, obj, true);
  wa->repush_after_eviction(category);
}

static bool WorkloadAware_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) return false;

  WorkloadAware *wa = wa_of(cache);
  wa->forget(obj_id);
  cache_remove_obj_base(cache, obj, true);
  // The removed block may have been a class head that the heap still points at;
  // the stale-entry check in choose_victim() re-derives heads on pop, so no
  // repair is needed here.
  return true;
}

static void WorkloadAware_set_request_ctx(cache_t *cache,
                                          const cache_request_ctx_t *ctx) {
  wa_of(cache)->cur_category = ctx->category;
}

static void WorkloadAware_record_request(cache_t *cache, const obj_id_t *ids,
                                         int64_t n) {
  WorkloadAware *wa = wa_of(cache);
  wa->hooked = true;

  for (int64_t i = 0; i < n; ++i) {
    const obj_id_t id = ids[i];
    auto it = wa->meta.find(id);

    if (it != wa->meta.end()) {
      // A reuse of a tracked block: learn this class's reuse gap and the global
      // lifespan, then move the block to its class's MRU end. Only a resident
      // block has a slot in that list; one recorded a moment ago and not yet
      // admitted is reordered by the insert instead.
      const int64_t gap = wa->now - it->second.last_access;
      if (gap > 0) {
        eviction::CategoryQueue &q = wa->queue_for(it->second.category);
        q.mean_gap = (1.0 - eviction::kEwmaAlpha) * q.mean_gap +
                     eviction::kEwmaAlpha * static_cast<double>(gap);
        wa->life = (1.0 - eviction::kEwmaAlpha) * wa->life +
                   eviction::kEwmaAlpha * static_cast<double>(gap);
        if (it->second.resident) {
          q.lru.splice(q.lru.end(), q.lru, it->second.slot);
        }
      }
      it->second.last_access = wa->now;
    } else {
      // First sighting. The block is not resident yet -- phase 3 inserts it --
      // so its class and offset are recorded now, and the insert files it into
      // the class's LRU list once it really is in the cache.
      eviction::BlockMeta m;
      m.category = wa->cur_category;
      m.last_access = wa->now;
      m.offset = i;
      wa->meta.emplace(id, m);
    }

    ++wa->now;
  }

  // The clock moved, so every cached reuse probability is stale.
  wa->heap_clock = -1;
}

#ifdef __cplusplus
}
#endif

/* AsymCache: the "computational-aware block evictor" of Multi-Segment Attention
 * (arXiv:2606.02964), §4.2-4.5, Algorithm 1. The paper names the concrete policy
 * MSA; AsymCache is the system it lives in, and both names resolve here.
 *
 * Evicts the block with the least *expected recomputation cost* (Eq. 3):
 *
 *     block <- argmin_B  E(B, t) = f_B(t) * dT_B
 *
 * dT_B -- the recompute cost (Eq. 7)
 * --------------------------------
 * Eq. 5 derives the cost of dropping one block as a function of the block's
 * positional index (l1+q1), i.e. how many blocks precede it, and Eq. 7's
 * approximation makes it exactly linear in that index:
 *
 *     dT_B = 2*k5*(l1+q1) + (k2 - k3 + k5)
 *
 * This implementation reads dT_B straight off the object's `cost`, which is what
 * the harness already computes per block position. Under prefixsim's
 * qwen3coder30b_blksz_16 model that is 865 + 2*position -- the same linear shape,
 * with 2*k5 measured as 2. So the cost model *is* Eq. 7, and taking dT_B from it
 * rather than refitting keeps the policy honest about which hardware it was
 * calibrated on.
 *
 * A consequence worth stating because it is a property of the policy, not a
 * defect: under a uniform cost model dT_B is constant and, in the paper's own
 * words, "our algorithm degrades to the conventional LRU strategy". Running this
 * with --cost-model uniform measures LRU with extra steps.
 *
 * f_B(t) -- the frequency value (Eq. 9)
 * -------------------------------------
 * Deliberately *not* an access-frequency count. §4.4 rejects that form (it is
 * what Pensieve does) on two grounds: it mispredicts, and it violates the
 * order-preserving rule of Eq. 8, which is what buys the log-time eviction. What
 * the paper uses instead is a recency term -- a piecewise exponential of the time
 * since last access, tau:
 *
 *     f_B(tau) = min( exp(-tau/alpha) , exp(-(tau - tau0)/beta) )
 *
 * high through the block's lifespan, then steeply declining (Fig. 8). The three
 * parameters are pinned by a turning point at (lifespan L, reuse probability p)
 * and a slope-change ratio r:
 *
 *     alpha = -L / ln(p)      beta = alpha / r      tau0 = L * (1 - 1/r)
 *
 * which puts the turning point exactly at tau = L: both branches equal p there,
 * so the min() switches without a discontinuity.
 *
 * Why two heaps
 * -------------
 * §4.4's order-preserving rule (Eq. 8) says an exponential f keeps the *relative*
 * order of any two blocks' weights invariant over time, so a tree can hold them.
 * The piecewise function as a whole breaks the rule -- but each branch satisfies
 * it, so the paper keeps one tree per branch (Algorithm 1). Within a branch the
 * shared exp(-now/.) factor cancels, leaving a time-invariant log-weight:
 *
 *     branch 1 (tau <  L):  k(B) = ln(dT_B) + t_last(B)/alpha
 *     branch 2 (tau >= L):  k(B) = ln(dT_B) + (t_last(B) + tau0)/beta
 *
 * Eviction peeks both branch minima, compares their actual E, and takes the
 * smaller -- Algorithm 1 lines 7-9, with lambda = 1 (see deviations). Blocks
 * migrate branch 1 -> 2 lazily as they age past the turning point.
 *
 * Entries are invalidated lazily rather than deleted. Re-keying on every access
 * would cost one heap push per block-access, which on a multi-turn trace is
 * hundreds of millions of pushes; instead each entry carries the last-access it
 * was built from, and a mismatch surfacing at the top is re-keyed then.
 *
 * Two clocks
 * ----------
 * AsymCache advances its clock once per block access; AsymCacheTime advances it
 * with the request's wall-clock timestamp. This is not a cosmetic choice -- it
 * decides whether the policy does anything at all. The paper defines tau and L on
 * real time (Fig. 7's axis is "Time to Last Access (s)", L is the P99 of that
 * CDF, r = 40 in their evaluation). On a block clock, L is fitted to the P99
 * *block* reuse gap, which on a large multi-turn trace is tens of millions of
 * blocks -- far beyond any cache. Then tau/alpha ~ 0 for every resident block, f
 * is flat, and E = f * dT_B collapses to cost-weighted LRU. The wall-clock
 * variant fits L in seconds (~180 s on the freeinference traces), so the lifespan
 * actually bites and blocks idle past it are steeply demoted.
 *
 * Deviations from the paper, both because a simulator has no offline profiler:
 *   - The online lambda rescaler of §5.1 (Algorithm 1 line 8) is not implemented;
 *     lambda stays at its initial 1, which the paper says makes the criterion
 *     identical to the plain frequency-value definition.
 *   - L is estimated from the reuse gaps observed before the first eviction
 *     instead of being pre-profiled. It is then frozen: the paper treats lifespan
 *     as fixed and adapts only via lambda, and refitting L would reorder both
 *     trees underneath the invariant the whole structure rests on.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <queue>
#include <unordered_map>
#include <vector>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/evictionAlgo.h"

namespace eviction {
namespace {

/// Cap on reuse-gap samples kept for the P99 lifespan estimate. The estimate is
/// a quantile, so more samples than this buy no accuracy, only memory.
constexpr size_t kGapSampleCap = 200000;

struct BlockMeta {
  double last_access = 0.0;
  /// dT_B, the recompute cost. Taken from the object's cost at insert.
  double cost = 1.0;
  /// Non-resident between record_request (which learns a last-access) and the
  /// insert that admits the block. Only resident blocks may be evicted.
  bool resident = false;
};

/// A lazily-invalidated heap entry. `snapshot` is the last-access the key was
/// computed from; if the block has been accessed since, the entry is stale.
struct HeapEntry {
  double key;
  uint64_t seq;
  obj_id_t id;
  double snapshot;

  /// Inverted so std::priority_queue (a max-heap) surfaces the smallest key.
  bool operator<(const HeapEntry &rhs) const {
    if (key != rhs.key) return key > rhs.key;
    return seq > rhs.seq;
  }
};

class AsymCache {
 public:
  /// Turning-point reuse probability p. The paper recommends 0.3-0.7.
  double reuse_prob = 0.5;
  /// Slope-change ratio r at the turning point.
  double slope_ratio = 10.0;
  /// Lifespan to fall back on when no reuse gap was observed before the first
  /// eviction, in this clock's units.
  double default_lifespan = 2.0e5;
  /// True for the wall-clock variant, which takes `now` from the request
  /// timestamp instead of counting block accesses.
  bool wall_clock = false;
  /// Set once record_request fires. Until then nothing advances the block clock
  /// and set_request_ctx never supplies a wall clock, so tau would be 0 for every
  /// block and the policy would decay to insertion order. A caller that drives
  /// the cache one object at a time gets a per-access clock instead.
  bool hooked = false;

  double now = 0.0;

  /// Frozen decay parameters; see the header comment for why they are frozen.
  bool frozen = false;
  double L = 0.0;
  double alpha = 1.0;
  double beta = 1.0;
  double tau0 = 0.0;
  std::vector<double> gaps;

  std::unordered_map<obj_id_t, BlockMeta> meta;
  /// Resident blocks, so freezing and the last-resort fallback can enumerate the
  /// cache without walking every block ever seen.
  int64_t n_resident = 0;

  std::priority_queue<HeapEntry> h1;
  std::priority_queue<HeapEntry> h2;
  uint64_t seq = 0;

  double tau_of(const BlockMeta &m) const {
    const double tau = now - m.last_access;
    return tau > 0.0 ? tau : 0.0;
  }

  /// f_B(tau), Eq. 9.
  double f_reuse(const BlockMeta &m) const {
    const double tau = tau_of(m);
    const double seg1 = std::exp(-tau / alpha);
    // Overflows to +inf when tau is far below the turning point, and min() then
    // picks seg1 -- which is the correct branch there anyway.
    const double seg2 = std::exp(-(tau - tau0) / beta);
    return seg1 < seg2 ? seg1 : seg2;
  }

  /// E(B, t) = f_B(t) * dT_B, Eq. 3.
  double expected_cost(const BlockMeta &m) const { return f_reuse(m) * m.cost; }

  int seg_of(const BlockMeta &m) const { return tau_of(m) < L ? 1 : 2; }

  double key_of(const BlockMeta &m, int seg) const {
    // ln(dT_B) so the multiplicative weight becomes additive and cannot
    // overflow; guarded because a cost model may hand us 0.
    const double lc = std::log(std::max(m.cost, 1.0));
    return seg == 1 ? lc + m.last_access / alpha
                    : lc + (m.last_access + tau0) / beta;
  }

  void push(obj_id_t id, const BlockMeta &m) {
    const int seg = seg_of(m);
    HeapEntry e{key_of(m, seg), seq++, id, m.last_access};
    if (seg == 1) {
      h1.push(e);
    } else {
      h2.push(e);
    }
    // Stale entries accumulate because accesses do not delete them. Rebuild once
    // they outnumber the resident set several times over, which bounds memory
    // without paying a rebuild often.
    const size_t total = h1.size() + h2.size();
    if (total > 3 * static_cast<size_t>(n_resident) + 2048) compact();
  }

  void compact() {
    h1 = std::priority_queue<HeapEntry>();
    h2 = std::priority_queue<HeapEntry>();
    for (const auto &kv : meta) {
      if (!kv.second.resident) continue;
      const int seg = seg_of(kv.second);
      HeapEntry e{key_of(kv.second, seg), seq++, kv.first, kv.second.last_access};
      if (seg == 1) {
        h1.push(e);
      } else {
        h2.push(e);
      }
    }
  }

  /// Pin (alpha, beta, tau0) from the lifespan observed so far, then seed both
  /// heaps from the resident set. Called at the first eviction: everything before
  /// it is the fill phase, which is exactly the window the paper would profile.
  void freeze() {
    double lifespan = default_lifespan;
    if (!gaps.empty()) {
      std::sort(gaps.begin(), gaps.end());
      const size_t idx =
          std::min(gaps.size() - 1, static_cast<size_t>(0.99 * gaps.size()));
      lifespan = gaps[idx];
    }
    L = std::max(1.0, lifespan);
    alpha = -L / std::log(reuse_prob);
    beta = alpha / slope_ratio;
    tau0 = L * (1.0 - 1.0 / slope_ratio);
    gaps.clear();
    gaps.shrink_to_fit();
    frozen = true;
    compact();
  }

  /// Top valid entry of one branch, or false when the branch holds nothing live.
  /// Drops entries for gone blocks, re-keys ones accessed since, and re-files
  /// ones that have aged across the turning point.
  bool peek_valid(std::priority_queue<HeapEntry> &heap, int seg, obj_id_t &out) {
    while (!heap.empty()) {
      const HeapEntry top = heap.top();
      auto it = meta.find(top.id);

      if (it == meta.end() || !it->second.resident) {
        heap.pop();
        continue;
      }
      if (it->second.last_access != top.snapshot) {
        heap.pop();
        push(top.id, it->second);
        continue;
      }
      if (seg_of(it->second) != seg) {
        heap.pop();
        push(top.id, it->second);
        continue;
      }
      out = top.id;
      return true;
    }
    return false;
  }

  /// Algorithm 1's Evict: compare the two branch minima by their actual E.
  bool choose_victim(obj_id_t &out) {
    if (n_resident <= 0) return false;
    if (!frozen) freeze();

    obj_id_t c1 = 0;
    obj_id_t c2 = 0;
    const bool ok1 = peek_valid(h1, 1, c1);
    const bool ok2 = peek_valid(h2, 2, c2);

    if (ok1 && ok2) {
      out = expected_cost(meta[c1]) <= expected_cost(meta[c2]) ? c1 : c2;
      return true;
    }
    if (ok1) {
      out = c1;
      return true;
    }
    if (ok2) {
      out = c2;
      return true;
    }

    // Both branches drained while blocks remain: some entry was lost. Rebuild
    // from the resident set and retry once, rather than telling the caller there
    // is nothing to evict (which it treats as fatal).
    compact();
    if (peek_valid(h1, 1, c1)) {
      out = c1;
      return true;
    }
    if (peek_valid(h2, 2, c2)) {
      out = c2;
      return true;
    }
    for (const auto &kv : meta) {
      if (kv.second.resident) {
        out = kv.first;
        return true;
      }
    }
    return false;
  }

  void forget(obj_id_t id) {
    auto it = meta.find(id);
    if (it == meta.end()) return;
    if (it->second.resident) --n_resident;
    meta.erase(it);
  }

  /// Learn a reuse gap during the fill phase. After freezing, L is fixed, so the
  /// samples are no longer collected.
  void observe_gap(double gap) {
    if (frozen || gap <= 0.0 || gaps.size() >= kGapSampleCap) return;
    gaps.push_back(gap);
  }
};

}  // namespace
}  // namespace eviction

using eviction::AsymCache;

#ifdef __cplusplus
extern "C" {
#endif

cache_t *AsymCache_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);
cache_t *AsymCacheTime_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params);
static void AsymCache_free(cache_t *cache);
static bool AsymCache_get(cache_t *cache, const request_t *req);
static cache_obj_t *AsymCache_find(cache_t *cache, const request_t *req,
                                   bool update_cache);
static cache_obj_t *AsymCache_insert(cache_t *cache, const request_t *req);
static cache_obj_t *AsymCache_to_evict(cache_t *cache, const request_t *req);
static void AsymCache_evict(cache_t *cache, const request_t *req);
static bool AsymCache_remove(cache_t *cache, obj_id_t obj_id);
static void AsymCache_set_request_ctx(cache_t *cache,
                                      const cache_request_ctx_t *ctx);
static void AsymCache_record_request(cache_t *cache, const obj_id_t *ids,
                                     int64_t n);
static void AsymCache_parse_params(cache_t *cache,
                                   const char *cache_specific_params);

static inline AsymCache *ac_of(const cache_t *cache) {
  return reinterpret_cast<AsymCache *>(cache->eviction_params);
}

static const char *AsymCache_current_params(const AsymCache *p) {
  static char buf[256];
  snprintf(buf, sizeof(buf),
           "reuse-prob=%.4lf,slope-ratio=%.4lf,default-lifespan=%.4lf", p->reuse_prob,
           p->slope_ratio, p->default_lifespan);
  return buf;
}

static void AsymCache_parse_params(cache_t *cache,
                                   const char *cache_specific_params) {
  AsymCache *p = ac_of(cache);
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end = nullptr;

  while (params_str != nullptr && params_str[0] != '\0') {
    char *key = strsep(&params_str, "=");
    char *value = strsep(&params_str, ",");

    while (params_str != nullptr && *params_str == ' ') params_str++;

    if (strcasecmp(key, "reuse-prob") == 0) {
      p->reuse_prob = strtod(value, &end);
      if (p->reuse_prob <= 0.0 || p->reuse_prob >= 1.0) {
        // alpha = -L/ln(p) is undefined at p = 1 and negative outside (0, 1),
        // which would inverse the decay rather than just mistune it.
        ERROR("AsymCache reuse-prob must be in (0, 1), got %s\n", value);
      }
    } else if (strcasecmp(key, "slope-ratio") == 0) {
      p->slope_ratio = strtod(value, &end);
      if (p->slope_ratio <= 0.0) {
        ERROR("AsymCache slope-ratio must be > 0, got %s\n", value);
      }
    } else if (strcasecmp(key, "default-lifespan") == 0) {
      p->default_lifespan = strtod(value, &end);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", AsymCache_current_params(p));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name, key,
            AsymCache_current_params(p));
    }
  }

  free(old_params_str);
}

static cache_t *asym_cache_init_common(const common_cache_params_t ccache_params,
                                       const char *cache_specific_params,
                                       const char *name, bool wall_clock) {
  cache_t *cache = cache_struct_init(name, ccache_params, cache_specific_params);
  AsymCache *p = new AsymCache;
  cache->eviction_params = reinterpret_cast<void *>(p);

  cache->cache_free = AsymCache_free;
  cache->get = AsymCache_get;
  cache->find = AsymCache_find;
  cache->insert = AsymCache_insert;
  cache->evict = AsymCache_evict;
  cache->remove = AsymCache_remove;
  cache->to_evict = AsymCache_to_evict;
  cache->set_request_ctx = AsymCache_set_request_ctx;
  cache->record_request = AsymCache_record_request;

  p->wall_clock = wall_clock;
  if (wall_clock) {
    // The paper's evaluation values, which only make sense on a clock measured
    // in seconds: r = 40, and a fallback lifespan of the order of Fig. 7's P99.
    p->slope_ratio = 40.0;
    p->default_lifespan = 180.0;
  }

  if (cache_specific_params != nullptr) {
    AsymCache_parse_params(cache, cache_specific_params);
  }

  return cache;
}

cache_t *AsymCache_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params) {
  cache_t *cache = asym_cache_init_common(ccache_params, cache_specific_params,
                                          "AsymCache", false);
  cache->cache_init = AsymCache_init;
  return cache;
}

cache_t *AsymCacheTime_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  cache_t *cache = asym_cache_init_common(ccache_params, cache_specific_params,
                                          "AsymCacheTime", true);
  cache->cache_init = AsymCacheTime_init;
  return cache;
}

static void AsymCache_free(cache_t *cache) {
  delete ac_of(cache);
  cache_struct_free(cache);
}

static bool AsymCache_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

static cache_obj_t *AsymCache_find(cache_t *cache, const request_t *req,
                                   const bool update_cache) {
  // Recency comes from record_request, which is the only place that sees the
  // whole request. A find() must not touch it, so that the residency probe (and
  // an ordinary hit) cannot perturb the policy.
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  AsymCache *p = ac_of(cache);
  if (!p->hooked && update_cache) {
    // No request structure available: this access *is* the clock tick. The wall
    // clock has no source here either, so both variants fall back to counting
    // accesses and therefore behave alike.
    if (obj != nullptr) {
      auto it = p->meta.find(req->obj_id);
      if (it != p->meta.end()) {
        p->observe_gap(p->now - it->second.last_access);
        it->second.last_access = p->now;
        if (p->frozen) p->push(req->obj_id, it->second);
      }
    }
    p->now += 1.0;
  }

  return obj;
}

static cache_obj_t *AsymCache_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  if (obj == nullptr) return nullptr;

  AsymCache *p = ac_of(cache);
  auto it = p->meta.find(req->obj_id);

  if (it == p->meta.end()) {
    // No record_request ran (plain cachesim, the test suite): the block's
    // last-access is now, and dT_B is whatever the request declared.
    eviction::BlockMeta m;
    m.last_access = p->now;
    m.cost = static_cast<double>(req->cost);
    m.resident = true;
    p->meta.emplace(req->obj_id, m);
    ++p->n_resident;
    if (p->frozen) p->push(req->obj_id, m);
    return obj;
  }

  // dT_B is a property of the block's position in the prefix, which the harness
  // encodes in the request's cost. record_request cannot see it, so it is
  // captured here, at the one point where the block and its cost meet.
  it->second.cost = static_cast<double>(req->cost);
  if (!it->second.resident) {
    it->second.resident = true;
    ++p->n_resident;
  }
  if (p->frozen) p->push(req->obj_id, it->second);

  return obj;
}

static cache_obj_t *AsymCache_to_evict(cache_t *cache, const request_t *req) {
  (void)req;
  obj_id_t victim = 0;
  if (!ac_of(cache)->choose_victim(victim)) return nullptr;
  return hashtable_find_obj_id(cache->hashtable, victim);
}

static void AsymCache_evict(cache_t *cache, const request_t *req) {
  (void)req;
  AsymCache *p = ac_of(cache);

  obj_id_t victim = 0;
  if (!p->choose_victim(victim)) {
    ERROR("%s: asked to evict with an empty cache\n", cache->cache_name);
    return;
  }

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, victim);
  if (obj == nullptr) {
    // Bookkeeping drifted from the hash table. Drop the phantom rather than
    // corrupting the accounting, and let the caller retry.
    p->forget(victim);
    ERROR("%s: victim %llu is not in the hash table\n", cache->cache_name,
          static_cast<unsigned long long>(victim));
    return;
  }

  p->forget(victim);
  cache_evict_base(cache, obj, true);
}

static bool AsymCache_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) return false;

  ac_of(cache)->forget(obj_id);
  cache_remove_obj_base(cache, obj, true);
  // Any heap entry still pointing at this block is detected as stale on pop.
  return true;
}

static void AsymCache_set_request_ctx(cache_t *cache,
                                      const cache_request_ctx_t *ctx) {
  AsymCache *p = ac_of(cache);
  // Only the wall-clock variant has a "now" that the request itself defines; the
  // block-counting variant advances solely in record_request. Setting it here
  // rather than in record_request is what keeps eviction from lagging a request
  // behind the arrival it is making room for.
  if (p->wall_clock) p->now = ctx->timestamp;
}

static void AsymCache_record_request(cache_t *cache, const obj_id_t *ids,
                                     int64_t n) {
  AsymCache *p = ac_of(cache);
  p->hooked = true;

  for (int64_t i = 0; i < n; ++i) {
    const obj_id_t id = ids[i];
    auto it = p->meta.find(id);

    if (it != p->meta.end()) {
      p->observe_gap(p->now - it->second.last_access);
      it->second.last_access = p->now;
    } else {
      // First sighting. Not resident yet -- the insert admits it -- so only the
      // last-access is recorded here; the insert fills in dT_B.
      eviction::BlockMeta m;
      m.last_access = p->now;
      p->meta.emplace(id, m);
    }

    // The block clock ticks per access; the wall clock is set once per request,
    // in set_request_ctx, so every block of a request shares its timestamp.
    if (!p->wall_clock) p->now += 1.0;
  }
}

#ifdef __cplusplus
}
#endif

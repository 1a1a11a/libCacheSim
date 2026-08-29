/* Session-level eviction: the victim is a conversation, not a block.
 *
 * Every other policy in this tree picks one block to evict. These pick a
 * *session* and free the blocks only that session still wants. The motivation
 * is structural rather than aesthetic, and rests on two properties of the
 * freeinference traces:
 *
 *   - A session is a monotonically growing prefix. Between consecutive turns the
 *     median common prefix is 100% of the previous turn's blocks, and the median
 *     number of blocks dropped is exactly 1 (the partial boundary block, rehashed
 *     as tokens are appended). So a session is a well-defined object that grows.
 *   - Context sliding is rare but violent. At the 99.9th percentile a turn drops
 *     6,891 blocks at once, and at the maximum 12,921 -- the whole front of the
 *     context falling out of a 256k-token window. Those blocks are dead the
 *     instant the turn arrives. A block-level policy can only discover that by
 *     waiting for them to age out; a session-level one is told.
 *
 * ------------------------------------------------------------------ ownership
 * A block is NOT owned by one session: 25% of unique blocks in the July trace
 * appear in more than one session, covering 34% of all accesses, concentrated at
 * the prefix head (91% of accesses at offset < 64) but still 27% past offset 16k.
 * So "evict a session, free its blocks" would tear blocks out from under other
 * live conversations.
 *
 * The fix is a claim count. Each session claims the blocks of its current turn;
 * a block is resident exactly while at least one session claims it. Evicting a
 * session drops its claims, and only the blocks whose count reaches zero are
 * actually freed. What a session is "worth" evicting is therefore its *private*
 * blocks, which is also its size -- and that size moves as other sessions fork
 * off it or leave, which is precisely the variable-size object being modelled.
 *
 * The claim diff also handles sliding for free: a turn that drops 6,891 blocks
 * releases 6,891 claims, and any of those that no session else wants become
 * evictable immediately rather than in however long recency would have taken.
 *
 * ------------------------------------------------------------------- residency
 * Blocks stay in the ordinary hash table and go through cache_insert_base /
 * cache_evict_base, so `cache->find(block_id, false)` still answers "is this
 * block resident" and prefixsim's phase-1 probe, verify pass and hole dump work
 * unchanged. Only victim *selection* is session-granular. This is deliberate:
 * making the hash table session-keyed would leave nothing able to answer the one
 * question the simulator asks most.
 *
 * ------------------------------------------------------------------- prefixsim
 * These policies need to know which conversation a request belongs to, which
 * only a driver that knows request boundaries can say. They are registered for
 * `prefixsim` alone; under a driver that calls neither hook every request
 * degenerates to its own single-turn session, which is correct but pointless.
 *
 * Sizes are in BLOCKS throughout (prefixsim gives every block obj_size 1).
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/logging.h"

namespace eviction {
namespace {

struct SessionMeta {
  /// Blocks this session currently claims -- the block list of its latest turn.
  std::vector<obj_id_t> path;
  /// Logical clock (one tick per recorded request) of this session's last turn.
  int64_t last_access = 0;
  /// Turns served. Used by the S3FIFO-style promotion test.
  int32_t freq = 0;
  /// Index of the session's next request; INT64_MAX if it never returns.
  int64_t next_access = INT64_MAX;
  /// Summed recompute cost of the claimed blocks, for the cost-aware policy.
  double cost_sum = 0.0;
};

struct BlockInfo {
  int32_t claims = 0;
  /// Recompute cost, captured at insert where the block and its cost meet.
  int32_t cost = 1;
};

/// Shared machinery: claim bookkeeping, block freeing, the evict_n loop.
/// A subclass supplies only "which session loses".
class SessionCache {
 public:
  virtual ~SessionCache() = default;

  cache_t *cache = nullptr;
  std::unordered_map<uint64_t, SessionMeta> sessions;
  std::unordered_map<obj_id_t, BlockInfo> blocks;

  int64_t now = 0;
  uint64_t cur_session = 0;
  int64_t cur_next_access = INT64_MAX;
  /// Synthesised ids for traces without session_id, so each request stands alone.
  uint64_t synthetic_next = 1;

  // ---- policy hooks ----
  /// Choose the session to evict. Return false when none is available.
  virtual bool select_victim(uint64_t &out) = 0;
  /// A session has just served a turn. `is_new` distinguishes admission from reuse.
  virtual void on_touch(uint64_t /*session*/, bool /*is_new*/) {}
  /// A session has been evicted and erased.
  virtual void on_removed(uint64_t /*session*/) {}

  void set_ctx(const cache_request_ctx_t *ctx) {
    cur_session = ctx->session;
    cur_next_access = ctx->session_next_access;
    if (cur_session == 0) {
      WARN_ONCE(
          "session-level eviction with no session id in the trace: every "
          "request becomes its own session, which is correct but makes the "
          "policy equivalent to evicting whole requests. Use a trace with a "
          "session_id field.\n");
      cur_session = ++synthetic_next;
    }
  }

  /// Re-claim the arriving turn's blocks and release the ones it dropped.
  void record(const obj_id_t *ids, int64_t n) {
    ++now;
    SessionMeta &m = sessions[cur_session];
    const bool is_new = m.path.empty() && m.last_access == 0;

    // Re-claim the whole new path, then drop the whole old one. Claiming first
    // is what makes this safe: a block held by both turns goes +1 then -1 and
    // never touches zero, so it is never freed and re-admitted. Doing it in this
    // order also avoids computing a set difference -- at 656M block accesses,
    // building two hash sets per request costs more than the eviction does.
    // A context slide falls out correctly: nothing in the new path is released,
    // and the thousands of blocks left behind are dropped in the second loop.
    for (int64_t i = 0; i < n; ++i) claim(ids[i], m);
    for (const obj_id_t id : m.path) release(id, m);
    m.path.assign(ids, ids + n);

    m.last_access = now;
    m.next_access = cur_next_access;
    if (m.freq < 3) ++m.freq;  // saturating, as in S3FIFO
    on_touch(cur_session, is_new);
  }

  /// Record a block's cost as it is admitted, and bill it to its claimant.
  void note_insert(obj_id_t id, int32_t cost) {
    auto it = blocks.find(id);
    if (it == blocks.end()) return;  // not claimed: nothing to bill
    const int32_t before = it->second.cost;
    it->second.cost = cost > 0 ? cost : 1;
    auto s = sessions.find(cur_session);
    if (s != sessions.end()) s->second.cost_sum += it->second.cost - before;
  }

  /// Free blocks until `n` have gone. Returns how many were freed.
  int64_t evict_n(int64_t n) {
    int64_t freed = 0;
    while (freed < n) {
      uint64_t victim = 0;
      if (!select_victim(victim)) break;
      freed += drop_session(victim);
    }
    return freed;
  }

 protected:
  void claim(obj_id_t id, SessionMeta &m) {
    BlockInfo &b = blocks[id];
    ++b.claims;
    m.cost_sum += b.cost;
  }

  void release(obj_id_t id, SessionMeta &m) {
    auto it = blocks.find(id);
    if (it == blocks.end()) return;
    m.cost_sum -= it->second.cost;
    if (--it->second.claims <= 0) {
      free_block(id);
      blocks.erase(it);
    }
  }

  void free_block(obj_id_t id) {
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, id);
    if (obj != nullptr) cache_evict_base(cache, obj, true);
  }

  /// Evict one session: drop its claims, free what nobody else wants.
  int64_t drop_session(uint64_t session) {
    auto it = sessions.find(session);
    if (it == sessions.end()) return 0;

    int64_t freed = 0;
    for (const obj_id_t id : it->second.path) {
      auto b = blocks.find(id);
      if (b == blocks.end()) continue;
      if (--b->second.claims <= 0) {
        if (hashtable_find_obj_id(cache->hashtable, id) != nullptr) ++freed;
        free_block(id);
        blocks.erase(b);
      }
    }
    sessions.erase(it);
    on_removed(session);
    return freed;
  }
};

// ---------------------------------------------------------------- session LRU

class SessionLRU : public SessionCache {
 public:
  std::list<uint64_t> order;  // front = least recently used
  std::unordered_map<uint64_t, std::list<uint64_t>::iterator> slot;

  bool select_victim(uint64_t &out) override {
    if (order.empty()) return false;
    out = order.front();
    return true;
  }
  void on_touch(uint64_t s, bool is_new) override {
    if (is_new) {
      order.push_back(s);
      slot[s] = std::prev(order.end());
    } else {
      auto it = slot.find(s);
      if (it != slot.end()) order.splice(order.end(), order, it->second);
    }
  }
  void on_removed(uint64_t s) override {
    auto it = slot.find(s);
    if (it == slot.end()) return;
    order.erase(it->second);
    slot.erase(it);
  }
};

// ------------------------------------------------------------- session Belady

/// Evict the session whose next turn is furthest away -- the session-granular
/// oracle. Lazily invalidated max-heap: an entry is stale if the session is gone
/// or has been re-recorded since, which is detected on pop rather than prevented.
class SessionBelady : public SessionCache {
 public:
  struct Entry {
    int64_t next_access;
    int64_t snapshot;
    uint64_t session;
    bool operator<(const Entry &r) const { return next_access < r.next_access; }
  };
  std::priority_queue<Entry> heap;

  bool select_victim(uint64_t &out) override {
    while (!heap.empty()) {
      const Entry top = heap.top();
      auto it = sessions.find(top.session);
      if (it == sessions.end() || it->second.last_access != top.snapshot) {
        heap.pop();
        continue;
      }
      out = top.session;
      return true;
    }
    // Heap drained but sessions remain (all entries were stale): rebuild once.
    if (sessions.empty()) return false;
    for (const auto &kv : sessions) {
      heap.push(Entry{kv.second.next_access, kv.second.last_access, kv.first});
    }
    if (heap.empty()) return false;
    out = heap.top().session;
    return true;
  }
  void on_touch(uint64_t s, bool /*is_new*/) override {
    const SessionMeta &m = sessions[s];
    heap.push(Entry{m.next_access, m.last_access, s});
  }
};

// ------------------------------------------------------- session RandomCompute

/// Sample K sessions, evict the one with the lowest cost/recency. The score is
/// the session-level reading of RandomCompute: what it would cost to rebuild
/// this conversation, discounted by how long it has been idle. Cost is summed
/// over the whole claimed path rather than only the private blocks, because the
/// private set changes as other sessions fork and tracking it per session would
/// cost more than the ranking is worth.
class SessionRandomCompute : public SessionCache {
 public:
  int n_sample = 8;
  std::vector<uint64_t> pool;
  std::unordered_map<uint64_t, size_t> pos;
  std::mt19937_64 rng{1};

  bool select_victim(uint64_t &out) override {
    if (pool.empty()) return false;
    const size_t k =
        std::min(pool.size(), static_cast<size_t>(std::max(1, n_sample)));
    double best = 0.0;
    bool found = false;
    for (size_t i = 0; i < k; ++i) {
      const uint64_t s = pool[rng() % pool.size()];
      auto it = sessions.find(s);
      if (it == sessions.end()) continue;
      const double age = static_cast<double>(now - it->second.last_access);
      const double score = it->second.cost_sum / (age > 1.0 ? age : 1.0);
      if (!found || score < best) {
        best = score;
        out = s;
        found = true;
      }
    }
    if (!found) out = pool[0];
    return true;
  }
  void on_touch(uint64_t s, bool is_new) override {
    if (!is_new) return;
    pos[s] = pool.size();
    pool.push_back(s);
  }
  void on_removed(uint64_t s) override {
    auto it = pos.find(s);
    if (it == pos.end()) return;
    const size_t i = it->second;
    pool[i] = pool.back();
    pos[pool[i]] = i;
    pool.pop_back();
    pos.erase(it);
  }
};

// ------------------------------------------------------------ session S3FIFO

/// A small FIFO of newcomers, a main FIFO of sessions that proved themselves, and
/// a ghost of what the small queue threw away. A session seen again while in the
/// ghost is admitted straight to main. Sizes are counted in SESSIONS, not blocks:
/// the queues order conversations, and how many blocks each holds is the outer
/// layer's concern.
class SessionS3FIFO : public SessionCache {
 public:
  double small_ratio = 0.1;
  int32_t promote_threshold = 1;
  size_t ghost_cap = 0;  // 0 = size it like the resident set

  std::list<uint64_t> small, main;
  std::unordered_map<uint64_t, std::pair<int, std::list<uint64_t>::iterator>> where;
  std::list<uint64_t> ghost;
  std::unordered_set<uint64_t> ghost_set;

  bool in_ghost(uint64_t s) const { return ghost_set.count(s) != 0; }

  void push_ghost(uint64_t s) {
    if (ghost_set.insert(s).second) ghost.push_back(s);
    const size_t cap = ghost_cap != 0 ? ghost_cap : (small.size() + main.size() + 1);
    while (ghost.size() > cap) {
      ghost_set.erase(ghost.front());
      ghost.pop_front();
    }
  }

  void place(uint64_t s, int queue) {
    std::list<uint64_t> &q = queue == 0 ? small : main;
    q.push_back(s);
    where[s] = {queue, std::prev(q.end())};
  }

  void unlink(uint64_t s) {
    auto it = where.find(s);
    if (it == where.end()) return;
    (it->second.first == 0 ? small : main).erase(it->second.second);
    where.erase(it);
  }

  bool select_victim(uint64_t &out) override {
    // Bounded because every iteration either moves a session between queues or
    // returns; a session can be promoted at most once before it is a candidate.
    const size_t limit = 2 * (small.size() + main.size()) + 8;
    for (size_t guard = 0; guard <= limit; ++guard) {
      const bool prefer_small =
          !small.empty() &&
          (main.empty() ||
           static_cast<double>(small.size()) >
               small_ratio * static_cast<double>(small.size() + main.size()));

      if (prefer_small) {
        const uint64_t s = small.front();
        auto m = sessions.find(s);
        if (m != sessions.end() && m->second.freq > promote_threshold) {
          unlink(s);
          place(s, 1);  // proved itself: promote rather than evict
          continue;
        }
        out = s;
        return true;
      }
      if (main.empty()) return false;
      const uint64_t s = main.front();
      auto m = sessions.find(s);
      if (m != sessions.end() && m->second.freq > 0) {
        --m->second.freq;
        unlink(s);
        place(s, 1);  // reinsertion at the tail, one life spent
        continue;
      }
      out = s;
      return true;
    }
    if (!small.empty()) { out = small.front(); return true; }
    if (!main.empty()) { out = main.front(); return true; }
    return false;
  }

  void on_touch(uint64_t s, bool is_new) override {
    if (!is_new) return;  // a hit only bumps freq, which record() already did
    if (in_ghost(s)) {
      ghost_set.erase(s);
      place(s, 1);
    } else {
      place(s, 0);
    }
  }

  void on_removed(uint64_t s) override {
    auto it = where.find(s);
    const bool from_small = it != where.end() && it->second.first == 0;
    unlink(s);
    if (from_small) push_ghost(s);
  }
};

// ---------------------------------------------------------------- session ARC

/// ARC over sessions: T1 holds conversations seen once, T2 those seen again, and
/// the ghosts B1/B2 steer `p`, the share of the resident set given to recency.
/// The target size is counted in sessions and tracks the resident set, because
/// how many conversations fit is decided by the outer block budget, not here.
class SessionARC : public SessionCache {
 public:
  std::list<uint64_t> t1, t2, b1, b2;
  std::unordered_map<uint64_t, std::pair<int, std::list<uint64_t>::iterator>> where;
  double p = 0.0;

  std::list<uint64_t> &listof(int k) {
    return k == 0 ? t1 : (k == 1 ? t2 : (k == 2 ? b1 : b2));
  }
  void place(uint64_t s, int k) {
    std::list<uint64_t> &q = listof(k);
    q.push_back(s);
    where[s] = {k, std::prev(q.end())};
  }
  void unlink(uint64_t s) {
    auto it = where.find(s);
    if (it == where.end()) return;
    listof(it->second.first).erase(it->second.second);
    where.erase(it);
  }
  int locate(uint64_t s) const {
    auto it = where.find(s);
    return it == where.end() ? -1 : it->second.first;
  }
  void trim_ghosts() {
    const size_t c = t1.size() + t2.size() + 1;
    while (b1.size() + b2.size() > c) {
      if (!b1.empty()) { where.erase(b1.front()); b1.pop_front(); }
      else if (!b2.empty()) { where.erase(b2.front()); b2.pop_front(); }
      else break;
    }
  }

  bool select_victim(uint64_t &out) override {
    if (t1.empty() && t2.empty()) return false;
    const bool from_t1 =
        !t1.empty() && (t2.empty() || static_cast<double>(t1.size()) > p);
    out = from_t1 ? t1.front() : t2.front();
    return true;
  }

  void on_touch(uint64_t s, bool /*is_new*/) override {
    const int k = locate(s);
    const double n1 = static_cast<double>(b1.size());
    const double n2 = static_cast<double>(b2.size());
    const double c = static_cast<double>(t1.size() + t2.size() + 1);

    if (k == 0 || k == 1) {          // resident: promote to T2 MRU
      unlink(s);
      place(s, 1);
    } else if (k == 2) {             // ghost hit in B1: favour recency
      p = std::min(c, p + std::max(1.0, n2 / std::max(1.0, n1)));
      unlink(s);
      place(s, 1);
    } else if (k == 3) {             // ghost hit in B2: favour frequency
      p = std::max(0.0, p - std::max(1.0, n1 / std::max(1.0, n2)));
      unlink(s);
      place(s, 1);
    } else {                         // brand new
      place(s, 0);
    }
    trim_ghosts();
  }

  void on_removed(uint64_t s) override {
    const int k = locate(s);
    unlink(s);
    if (k == 0) place(s, 2);        // T1 -> B1
    else if (k == 1) place(s, 3);   // T2 -> B2
    trim_ghosts();
  }
};

}  // namespace
}  // namespace eviction

using eviction::SessionCache;

#ifdef __cplusplus
extern "C" {
#endif

static inline SessionCache *sc_of(const cache_t *cache) {
  return reinterpret_cast<SessionCache *>(cache->eviction_params);
}

static void SessionCache_free(cache_t *cache) {
  delete sc_of(cache);
  cache_struct_free(cache);
}

static bool SessionCache_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

static cache_obj_t *SessionCache_find(cache_t *cache, const request_t *req,
                                      const bool update_cache) {
  // Residency is an ordinary hash-table question; recency lives on the session,
  // and record_request is what updates it. A find must not disturb anything, so
  // that the phase-1 probe stays invisible.
  return cache_find_base(cache, req, update_cache);
}

static cache_obj_t *SessionCache_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  if (obj != nullptr) sc_of(cache)->note_insert(req->obj_id, req->cost);
  return obj;
}

static cache_obj_t *SessionCache_to_evict(cache_t *cache, const request_t *req) {
  (void)req;
  SessionCache *sc = sc_of(cache);
  uint64_t victim = 0;
  if (!sc->select_victim(victim)) return nullptr;
  auto it = sc->sessions.find(victim);
  if (it == sc->sessions.end()) return nullptr;
  for (const obj_id_t id : it->second.path) {
    auto b = sc->blocks.find(id);
    if (b != sc->blocks.end() && b->second.claims <= 1) {
      cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, id);
      if (obj != nullptr) return obj;
    }
  }
  return nullptr;
}

static void SessionCache_evict(cache_t *cache, const request_t *req) {
  (void)req;
  // One "eviction" is one session's worth of private blocks, which may be zero
  // if everything it holds is shared. Keep going until something is actually
  // freed, so the caller's occupancy check can make progress.
  if (sc_of(cache)->evict_n(1) == 0) {
    ERROR("session eviction freed nothing: no session holds a private block\n");
  }
}

static int64_t SessionCache_evict_n(cache_t *cache, const request_t *req,
                                    int64_t n) {
  (void)req;
  return sc_of(cache)->evict_n(n);
}

static bool SessionCache_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) return false;
  // Drop every claim on this block; the sessions holding it keep the stale id in
  // their path, which drop_session tolerates (the block is simply already gone).
  sc_of(cache)->blocks.erase(obj_id);
  cache_remove_obj_base(cache, obj, true);
  return true;
}

static void SessionCache_set_request_ctx(cache_t *cache,
                                         const cache_request_ctx_t *ctx) {
  sc_of(cache)->set_ctx(ctx);
}

static void SessionCache_record_request(cache_t *cache, const obj_id_t *ids,
                                        int64_t n) {
  sc_of(cache)->record(ids, n);
}

static cache_t *session_cache_init(const common_cache_params_t ccache_params,
                                   const char *cache_specific_params,
                                   const char *name, SessionCache *impl,
                                   cache_init_func_ptr self) {
  cache_t *cache = cache_struct_init(name, ccache_params, cache_specific_params);
  cache->eviction_params = reinterpret_cast<void *>(impl);
  impl->cache = cache;

  cache->cache_init = self;
  cache->cache_free = SessionCache_free;
  cache->get = SessionCache_get;
  cache->find = SessionCache_find;
  cache->insert = SessionCache_insert;
  cache->evict = SessionCache_evict;
  cache->evict_n = SessionCache_evict_n;
  cache->remove = SessionCache_remove;
  cache->to_evict = SessionCache_to_evict;
  cache->set_request_ctx = SessionCache_set_request_ctx;
  cache->record_request = SessionCache_record_request;

  return cache;
}

cache_t *SessionLRU_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  return session_cache_init(ccache_params, cache_specific_params, "SessionLRU",
                            new eviction::SessionLRU, SessionLRU_init);
}

cache_t *SessionBelady_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  return session_cache_init(ccache_params, cache_specific_params,
                            "SessionBelady", new eviction::SessionBelady,
                            SessionBelady_init);
}

cache_t *SessionARC_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  return session_cache_init(ccache_params, cache_specific_params, "SessionARC",
                            new eviction::SessionARC, SessionARC_init);
}

cache_t *SessionS3FIFO_init(const common_cache_params_t ccache_params,
                            const char *cache_specific_params) {
  return session_cache_init(ccache_params, cache_specific_params,
                            "SessionS3FIFO", new eviction::SessionS3FIFO,
                            SessionS3FIFO_init);
}

cache_t *SessionRandomCompute_init(const common_cache_params_t ccache_params,
                                   const char *cache_specific_params) {
  return session_cache_init(ccache_params, cache_specific_params,
                            "SessionRandomCompute",
                            new eviction::SessionRandomCompute,
                            SessionRandomCompute_init);
}

#ifdef __cplusplus
}
#endif

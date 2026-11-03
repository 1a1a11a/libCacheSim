// Sanity checks for the prefixsim three-phase serve loop, on LRU; a check that
// lhd_compute reduces to lhd under the uniform cost model; that a skipped
// request does not mislead the oracles; and that the partial-node prefix tree
// takes a promoted orphan back into its path.
//
// Each case is a hand-traceable trace where the correct number of block hits
// can be derived on paper. Requests are built directly from block ids rather
// than through a trace reader: in every case the shared blocks form a genuine
// common prefix, so raw ids and prefix-hashed ids give identical sharing, and
// using the ids verbatim keeps the expected values readable.
//
// Remember the two rules that make these traces work out:
//   - statistics are taken in phase 1, before any eviction for this request
//   - blocks are replayed deepest-first, so LRU's tail is the deep end

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "cache/eviction/cpp/prefixRadixTree.hpp"
#include "cost_model.hpp"
#include "simulator.hpp"
#include "trace.hpp"
#include "utils/include/mymath.h"

namespace {

struct TestCase {
  const char *name;
  int64_t cache_size;
  std::vector<std::vector<obj_id_t>> requests;
  int64_t expected_hits;
  int64_t expected_blocks;
  /// Self-evictions with phase-2 pinning on (the default) and off. Pinning
  /// makes the arriving request's own blocks unevictable, so the pinned count
  /// is 0 wherever LRU is fully pin-aware; the unpinned count is what the
  /// simulator did before pinning existed, and is kept as a regression on
  /// --no-pin still reproducing the old behaviour. The hit count is the same
  /// either way: statistics are taken in phase 1, before any eviction.
  int64_t expected_self_evictions_pinned;
  int64_t expected_self_evictions_unpinned;
  /// Requests too large to ever be fully resident. Defaulted, so the cases that
  /// predate this check need no edit.
  int64_t expected_skipped = 0;
};

std::vector<prefixsim::Request> build(
    const std::vector<std::vector<obj_id_t>> &raw) {
  std::vector<prefixsim::Request> requests;
  requests.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    prefixsim::Request request;
    request.index = static_cast<int64_t>(i);
    request.timestamp = static_cast<double>(i);
    request.blocks = raw[i];
    requests.push_back(request);
  }
  prefixsim::annotate_next_access(requests);
  return requests;
}

bool run_case(const TestCase &tc, bool pin) {
  const std::vector<prefixsim::Request> requests = build(tc.requests);
  const int64_t expected_self_evictions =
      pin ? tc.expected_self_evictions_pinned
          : tc.expected_self_evictions_unpinned;
  const char *mode = pin ? "pin" : "no-pin";

  cache_t *cache = prefixsim::create_cache_by_name("lru", tc.cache_size);
  if (cache == nullptr) {
    printf("  [%s/%s] FAIL: could not create LRU cache\n", tc.name, mode);
    return false;
  }

  prefixsim::SimulatorConfig config;
  config.cache_size_blocks = tc.cache_size;
  config.cost_model = prefixsim::CostModel();  // uniform
  config.verify = true;
  config.pin_request_blocks = pin;

  bool ok = true;
  std::string error;
  {
    prefixsim::Simulator sim(cache, config);
    if (!sim.run(requests, error)) {
      printf("  [%s/%s] FAIL: %s\n", tc.name, mode, error.c_str());
      ok = false;
    } else {
      const prefixsim::Stats &st = sim.stats();

      printf(
          "  [%s/%-6s] hits %lld/%lld = %.4f (expected %lld/%lld = %.4f), "
          "self-evict %lld, unexpected-evict %lld\n",
          tc.name, mode, static_cast<long long>(st.n_block_hits),
          static_cast<long long>(st.n_blocks), st.block_hit_ratio(),
          static_cast<long long>(tc.expected_hits),
          static_cast<long long>(tc.expected_blocks),
          static_cast<double>(tc.expected_hits) /
              static_cast<double>(tc.expected_blocks),
          static_cast<long long>(st.n_self_evictions),
          static_cast<long long>(st.n_unexpected_evictions));

      if (st.n_blocks != tc.expected_blocks) {
        printf("  [%s/%s] FAIL: block count %lld, expected %lld\n", tc.name,
               mode, static_cast<long long>(st.n_blocks),
               static_cast<long long>(tc.expected_blocks));
        ok = false;
      }
      if (st.n_block_hits != tc.expected_hits) {
        printf("  [%s/%s] FAIL: hit count %lld, expected %lld\n", tc.name, mode,
               static_cast<long long>(st.n_block_hits),
               static_cast<long long>(tc.expected_hits));
        ok = false;
      }
      if (st.n_self_evictions != expected_self_evictions) {
        printf("  [%s/%s] FAIL: self-eviction count %lld, expected %lld\n",
               tc.name, mode, static_cast<long long>(st.n_self_evictions),
               static_cast<long long>(expected_self_evictions));
        ok = false;
      }
      if (st.n_requests_skipped != tc.expected_skipped) {
        printf("  [%s/%s] FAIL: skipped %lld requests, expected %lld\n",
               tc.name, mode, static_cast<long long>(st.n_requests_skipped),
               static_cast<long long>(tc.expected_skipped));
        ok = false;
      }
      // Phase 2 must reserve exactly enough room, always.
      if (st.n_unexpected_evictions != 0) {
        printf("  [%s/%s] FAIL: %lld evictions during replay, expected 0\n",
               tc.name, mode,
               static_cast<long long>(st.n_unexpected_evictions));
        ok = false;
      }
      // With the uniform cost model the two headline metrics must coincide.
      if (st.compute_saving_ratio() != st.block_hit_ratio()) {
        printf(
            "  [%s/%s] FAIL: uniform cost but compute saving %.6f != hit ratio "
            "%.6f\n",
            tc.name, mode, st.compute_saving_ratio(), st.block_hit_ratio());
        ok = false;
      }
    }
  }  // Simulator detaches its recorder before the cache is freed.

  cache->cache_free(cache);
  return ok;
}

/// lhd_compute weights LHD's hit density by each block's cost, so under the
/// uniform cost model it must serve exactly as lhd does. Both sample from
/// libCacheSim's RNG, which is reseeded before each run. The trace is random
/// but fixed: 40 conversations behind a shared system prompt, re-read at random
/// lengths, through a cache of about a tenth of them.
bool lhd_compute_matches_lhd() {
  std::vector<std::vector<obj_id_t>> raw;
  uint64_t state = 1;
  auto next = [&state]() {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state >> 33;
  };
  for (int i = 0; i < 4000; ++i) {
    const obj_id_t conversation = next() % 40;
    const uint64_t length = 1 + next() % 30;
    std::vector<obj_id_t> blocks = {1, 2, 3};  // the system prompt
    for (uint64_t k = 0; k < length; ++k) {
      blocks.push_back(1000 * (conversation + 1) + k);
    }
    raw.push_back(blocks);
  }
  const std::vector<prefixsim::Request> requests = build(raw);
  const int64_t cache_size = 128;

  const char *algos[] = {"lhd", "lhd_compute"};
  prefixsim::Stats stats[2];
  for (int a = 0; a < 2; ++a) {
    set_rand_seed(42);
    cache_t *cache = prefixsim::create_cache_by_name(algos[a], cache_size);
    if (cache == nullptr) {
      printf("  [%s] FAIL: could not create the cache\n", algos[a]);
      return false;
    }
    prefixsim::SimulatorConfig config;
    config.cache_size_blocks = cache_size;
    config.verify = true;
    std::string error;
    bool ok = true;
    {
      prefixsim::Simulator sim(cache, config);
      ok = sim.run(requests, error);
      stats[a] = sim.stats();
    }
    cache->cache_free(cache);
    if (!ok) {
      printf("  [%s] FAIL: %s\n", algos[a], error.c_str());
      return false;
    }
    printf("  [%s] hits %lld/%lld, evictions %lld, self-evict %lld\n", algos[a],
           static_cast<long long>(stats[a].n_block_hits),
           static_cast<long long>(stats[a].n_blocks),
           static_cast<long long>(stats[a].n_evictions),
           static_cast<long long>(stats[a].n_self_evictions));
  }
  if (stats[0].n_block_hits != stats[1].n_block_hits ||
      stats[0].n_evictions != stats[1].n_evictions ||
      stats[0].n_self_evictions != stats[1].n_self_evictions) {
    printf("  FAIL: lhd_compute under the uniform cost differs from lhd\n");
    return false;
  }
  return true;
}

/// A request larger than the cache is skipped, so annotation must leave it out:
/// an oracle that sees it plans for a reuse that never happens. Each oracle
/// must score a trace holding such a request exactly as it scores the trace
/// with that request deleted.
///
/// [1] [2] [1,3,4] [5] [2] at capacity 2, sessions 1 2 1 3 2; [1,3,4] cannot
/// fit. Counting it, block 1 and session 1 look reused, so at [5] Belady keeps
/// them and evicts block 2 (session 2), and the last request misses. Without
/// it, nothing of block 1 or session 1 is reused, so they go and [2] hits.
bool skipped_requests_do_not_mislead_oracles() {
  const int64_t cache_size = 2;
  const std::vector<std::vector<obj_id_t>> blocks = {
      {1}, {2}, {1, 3, 4}, {5}, {2}};
  const std::vector<uint64_t> sessions = {1, 2, 1, 3, 2};

  auto make = [&](bool keep_oversized) {
    std::vector<prefixsim::Request> requests;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (!keep_oversized && i == 2) continue;
      prefixsim::Request request;
      request.index = static_cast<int64_t>(i);
      request.timestamp = static_cast<double>(i);
      request.blocks = blocks[i];
      request.session = sessions[i];
      requests.push_back(request);
    }
    prefixsim::annotate_next_access(requests, cache_size);
    prefixsim::annotate_session_next_access(requests, cache_size);
    return requests;
  };
  const std::vector<prefixsim::Request> with_skip = make(true);
  const std::vector<prefixsim::Request> prefiltered = make(false);

  bool ok = true;
  for (const char *algo : {"belady", "belady_compute", "session_belady"}) {
    prefixsim::Stats stats[2];
    for (int k = 0; k < 2; ++k) {
      cache_t *cache = prefixsim::create_cache_by_name(algo, cache_size);
      prefixsim::SimulatorConfig config;
      config.cache_size_blocks = cache_size;
      std::string error;
      bool run_ok = true;
      {
        prefixsim::Simulator sim(cache, config);
        run_ok = sim.run(k == 0 ? with_skip : prefiltered, error);
        stats[k] = sim.stats();
      }
      cache->cache_free(cache);
      if (!run_ok) {
        printf("  [%s] FAIL: %s\n", algo, error.c_str());
        return false;
      }
    }
    printf("  [%s] hits %lld with the skipped request, %lld without it\n", algo,
           static_cast<long long>(stats[0].n_block_hits),
           static_cast<long long>(stats[1].n_block_hits));
    if (stats[0].n_requests_skipped != 1 || stats[1].n_block_hits != 1 ||
        stats[0].n_block_hits != stats[1].n_block_hits ||
        stats[0].n_evictions != stats[1].n_evictions) {
      printf("  [%s] FAIL: expected 1 skipped request and 1 hit in both\n",
             algo);
      ok = false;
    }
  }
  return ok;
}

/// The partial-node prefix tree must not keep a block both in a node and among
/// its orphans. This is how partial_node_random_compute_small_queue got one: a
/// block waiting in the small queue is not resident in the tree, so its leaf
/// is pruned when the leaf's last resident block goes. Promoted afterwards, the
/// block has no node and becomes an orphan. A later request on the same path
/// re-creates the node. If the orphan entry survived that, evicting orphans
/// drew it forever: one eviction removed it from the cache but not from the
/// orphans, and every later draw found nothing to evict.
bool orphan_rejoins_its_path() {
  eviction::PrefixRadixTree tree;
  const obj_id_t path[] = {1, 2};
  tree.add_sequence(path, 2);
  tree.mark_resident(1);  // block 2 waits in the small queue
  tree.mark_evicted(1);   // the leaf has nothing resident left: pruned
  tree.mark_resident(2);  // 2 is promoted, with no node to go to
  const bool orphaned = tree.has_orphans();
  tree.add_sequence(path, 2);  // the path is requested again

  bool ok = true;
  if (!orphaned) {
    printf("  FAIL: block 2 was not an orphan after its leaf was pruned\n");
    ok = false;
  }
  if (tree.has_orphans() || !tree.is_resident(2) || tree.is_resident(1) ||
      tree.n_sampleable() != 1) {
    printf("  FAIL: re-recording the path did not take block 2 back in\n");
    ok = false;
  }
  tree.mark_evicted(2);
  if (tree.has_orphans() || tree.n_sampleable() != 0) {
    printf("  FAIL: evicting block 2 left it behind\n");
    ok = false;
  }
  return ok;
}

}  // namespace

int main(void) {
  const std::vector<TestCase> cases = {
      // (a) Req2 reuses the whole prefix of Req1 and adds one new block. The
      //     cache is exactly full, so one slot must be freed. The only block
      //     Req2 does not want is 5, and reverse-order replay left it at the
      //     LRU tail, so it is evicted first and nothing is self-evicted.
      {"a", 5, {{1, 2, 3, 4, 5}, {1, 2, 3, 4, 6}}, 4, 10, 0, 0},

      // (b) Same shape, but the block Req3 does not want (1) was admitted by an
      //     earlier, unrelated request and is the oldest thing in the cache.
      {"b", 5, {{1}, {2, 3, 4, 5}, {2, 3, 4, 5, 6}}, 4, 10, 0, 0},

      // (c) Now the unwanted block (5) is the *most recently* used, so LRU
      //     reaches it only after walking through all four blocks Req3 wants.
      //     Unpinned, those four are self-evicted and re-inserted (the hit
      //     count is unaffected either way, being taken in phase 1). Pinned,
      //     they are skipped over and only block 5 goes: 0 self-evictions.
      {"c", 5, {{1, 2, 3, 4}, {5}, {1, 2, 3, 4, 6}}, 4, 10, 0, 4},

      // (d) As (c), plus a single-block request that re-touches block 1.
      //     Req2 {5} arrives with the cache at 4/5, so it evicts nothing and
      //     block 1 is still resident for Req3 -- that is the 5th hit. Req3
      //     inserts nothing in turn, so blocks 1-4 are all still resident for
      //     Req4, which hits 4 times: 1 + 4 = 5 over 4+1+1+5 = 11 blocks.
      //     Req3 also promotes block 1 past block 5 in recency, so unpinned
      //     LRU reaches the unwanted block 5 one eviction sooner than in (c):
      //     3 self-evictions instead of 4. Pinned, again 0.
      {"d", 5, {{1, 2, 3, 4}, {5}, {1}, {1, 2, 3, 4, 6}}, 5, 11, 0, 3},

      // (e) Req2 wants 4 distinct blocks but the cache holds 3, so it can never
      //     be fully resident and is skipped with a warning. Two things must
      //     hold, and the third request is what proves the second: the skipped
      //     request contributes nothing to the ratios (6 blocks counted, not
      //     10), and it does not disturb the cache -- Req3 finds all of 1,2,3
      //     still resident and hits 3 times. A skip that evicted on the way out
      //     would show up here as a lower hit count.
      {"e", 3, {{1, 2, 3}, {1, 2, 3, 4}, {1, 2, 3}}, 3, 6, 0, 0, 1},
  };

  printf("prefixsim LRU sanity checks\n");
  int n_failed = 0;
  size_t n_run = 0;
  // Both modes: pinning must remove the self-evictions without moving a single
  // hit, and --no-pin must still reproduce the pre-pinning behaviour exactly.
  for (bool pin : {true, false}) {
    for (const TestCase &tc : cases) {
      ++n_run;
      if (!run_case(tc, pin)) ++n_failed;
    }
  }

  printf("lhd_compute under the uniform cost\n");
  ++n_run;
  if (!lhd_compute_matches_lhd()) ++n_failed;

  printf("a skipped request and the oracles\n");
  ++n_run;
  if (!skipped_requests_do_not_mislead_oracles()) ++n_failed;

  printf("a promoted orphan rejoins its path\n");
  ++n_run;
  if (!orphan_rejoins_its_path()) ++n_failed;

  if (n_failed != 0) {
    printf("%d/%zu case(s) FAILED\n", n_failed, n_run);
    return 1;
  }
  printf("all %zu cases passed\n", n_run);
  return 0;
}

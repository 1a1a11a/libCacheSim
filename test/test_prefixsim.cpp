// Sanity checks for the prefixsim three-phase serve loop, on LRU.
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

#include "cost_model.hpp"
#include "simulator.hpp"
#include "trace.hpp"

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

std::vector<prefixsim::Request> build(const std::vector<std::vector<obj_id_t>> &raw) {
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
      pin ? tc.expected_self_evictions_pinned : tc.expected_self_evictions_unpinned;
  const char *mode = pin ? "pin" : "no-pin";

  cache_t *cache = prefixsim::create_cache_by_name("lru", tc.cache_size);
  if (cache == nullptr) {
    printf("  [%s/%s] FAIL: could not create LRU cache\n", tc.name, mode);
    return false;
  }

  prefixsim::SimulatorConfig config;
  config.cache_size_blocks = tc.cache_size;
  config.cost_model = prefixsim::CostModel::kUniform;
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

      printf("  [%s/%-6s] hits %lld/%lld = %.4f (expected %lld/%lld = %.4f), "
             "self-evict %lld, unexpected-evict %lld\n",
             tc.name, mode, static_cast<long long>(st.n_block_hits),
             static_cast<long long>(st.n_blocks), st.block_hit_ratio(),
             static_cast<long long>(tc.expected_hits),
             static_cast<long long>(tc.expected_blocks),
             static_cast<double>(tc.expected_hits) / static_cast<double>(tc.expected_blocks),
             static_cast<long long>(st.n_self_evictions),
             static_cast<long long>(st.n_unexpected_evictions));

      if (st.n_blocks != tc.expected_blocks) {
        printf("  [%s/%s] FAIL: block count %lld, expected %lld\n", tc.name, mode,
               static_cast<long long>(st.n_blocks),
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
        printf("  [%s/%s] FAIL: self-eviction count %lld, expected %lld\n", tc.name, mode,
               static_cast<long long>(st.n_self_evictions),
               static_cast<long long>(expected_self_evictions));
        ok = false;
      }
      if (st.n_requests_skipped != tc.expected_skipped) {
        printf("  [%s/%s] FAIL: skipped %lld requests, expected %lld\n", tc.name, mode,
               static_cast<long long>(st.n_requests_skipped),
               static_cast<long long>(tc.expected_skipped));
        ok = false;
      }
      // Phase 2 must reserve exactly enough room, always.
      if (st.n_unexpected_evictions != 0) {
        printf("  [%s/%s] FAIL: %lld evictions during replay, expected 0\n", tc.name, mode,
               static_cast<long long>(st.n_unexpected_evictions));
        ok = false;
      }
      // With the uniform cost model the two headline metrics must coincide.
      if (st.compute_saving_ratio() != st.block_hit_ratio()) {
        printf("  [%s/%s] FAIL: uniform cost but compute saving %.6f != hit ratio %.6f\n",
               tc.name, mode, st.compute_saving_ratio(), st.block_hit_ratio());
        ok = false;
      }
    }
  }  // Simulator detaches its recorder before the cache is freed.

  cache->cache_free(cache);
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

  if (n_failed != 0) {
    printf("%d/%zu case(s) FAILED\n", n_failed, n_run);
    return 1;
  }
  printf("all %zu cases passed\n", n_run);
  return 0;
}

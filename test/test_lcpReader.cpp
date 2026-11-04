// Checks for libCacheSim's side of the lcp trace format, on traces small
// enough to derive by hand:
//   - the flat lcp reader, on a file built here directly from the spec's
//     structs: block ids, derived cost, next access, timestamp in seconds, and
//     rewinding mid-request
//   - cost models: the named ones, linear:A,B and table:FILE, what each gives
//     every position, the specs refused, and the flat reader applying one
//   - a cached object whose cost changes between accesses keeps the latest
//
// The writers and the request-level reader are tested with prefixsim, in
// https://github.com/cacheMon/PrefixSim.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/llmCostModel.h"
#include "libCacheSim/reader.h"
#include "traceReader/customizedReader/lcp.h"
}

namespace {

int n_failed = 0;

#define CHECK_EQ(got, want)                                                    \
  do {                                                                         \
    const long long g_ = static_cast<long long>(got);                          \
    const long long w_ = static_cast<long long>(want);                         \
    if (g_ != w_) {                                                            \
      printf("  FAIL %s:%d: %s = %lld, want %lld\n", __FILE__, __LINE__, #got, \
             g_, w_);                                                          \
      ++n_failed;                                                              \
    }                                                                          \
  } while (0)

/// Two requests of one session and user, {1,2,3} completing at 1.5 s and
/// {1,2,4} at 2.7 s, each having arrived 0.5 s earlier, written straight from
/// the spec's structs.
void write_trace(const char *path) {
  lcp_trace_header_t h;
  memset(&h, 0, sizeof(h));
  h.start_magic = h.end_magic = LCP_TRACE_MAGIC;
  h.version = LCP_CURR_VERSION;
  h.stat.version = LCP_CURR_STAT_VERSION;
  h.stat.n_req = 2;
  h.stat.n_block = 6;
  h.stat.n_unique_block = 4;
  h.stat.n_session = 1;
  h.stat.n_user = 1;
  h.stat.max_blocks_per_req = 3;
  h.stat.block_size_tokens = 16;

  const uint64_t ids[2][3] = {{1, 2, 3}, {1, 2, 4}};
  // forward vtimes 1..6: id 1 recurs at 4, id 2 at 5, nothing else recurs,
  // so the next accesses are {4, 5, -1} and {-1, -1, -1}. The file stores
  // each as the difference from the previous block's in its request.
  const int64_t next_delta[2][3] = {{4, 1, -6}, {-1, 0, 0}};
  const int64_t ts_us[2] = {1500000, 2700000};
  const int64_t session_next[2] = {2, -1};  // 1-based

  FILE *f = fopen(path, "wb");
  fwrite(&h, sizeof(h), 1, f);
  for (int r = 0; r < 2; ++r) {
    lcp_req_v1_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.start_time_us = ts_us[r] - 500000;
    rec.complete_time_us = ts_us[r];
    rec.chat_id = rec.parent_chat_id = -1;
    rec.session_next_req = session_next[r];
    rec.session_id = 1;
    rec.user_id = 1;
    rec.n_blocks = 3;
    rec.input_length = rec.output_length = rec.turn = rec.type_id = -1;
    fwrite(&rec, sizeof(rec), 1, f);
    for (int i = 0; i < 3; ++i) {
      const lcp_block_t b = {ids[r][i], next_delta[r][i]};
      fwrite(&b, sizeof(b), 1, f);
    }
  }
  fclose(f);
}

const obj_id_t kWantId[6] = {1, 2, 3, 1, 2, 4};
const int64_t kWantNext[6] = {4,
                              5,
                              MAX_REUSE_DISTANCE,
                              MAX_REUSE_DISTANCE,
                              MAX_REUSE_DISTANCE,
                              MAX_REUSE_DISTANCE};

void test_flat_reader() {
  printf("flat reader\n");
  const char *path = "test_lcp_reader_tmp.lcp";
  write_trace(path);

  reader_init_param_t params;
  set_default_reader_init_params(&params);
  params.llm_cost_model = const_cast<char *>("position");
  reader_t *reader = setup_reader(path, LCP_TRACE, &params);
  CHECK_EQ(reader->n_total_req, 6);

  const int64_t want_cost[6] = {1, 2, 3, 1, 2, 3};
  const int64_t want_time[6] = {1, 1, 1, 2, 2, 2};
  request_t *req = new_request();

  // Rewind from inside the first request: the reader must not then take the
  // first request record for a block.
  read_one_req(reader, req);
  read_one_req(reader, req);
  reset_reader(reader);

  int n = 0;
  while (read_one_req(reader, req) == 0) {
    if (n < 6) {
      CHECK_EQ(req->obj_id, kWantId[n]);
      CHECK_EQ(req->obj_cost, want_cost[n]);
      CHECK_EQ(req->clock_time, want_time[n]);
      CHECK_EQ(req->next_access_vtime, kWantNext[n]);
    }
    ++n;
  }
  CHECK_EQ(n, 6);

  free_request(req);
  close_reader(reader);
  remove(path);
}

/// Costs a model gives positions 0 .. n - 1, or {-1} if it does not parse.
std::vector<int64_t> costs(const char *spec, int64_t n) {
  llm_cost_model_t model;
  char err[1024];
  if (!llm_cost_model_parse(spec, &model, err, sizeof(err))) return {-1};
  std::vector<int64_t> out;
  for (int64_t p = 0; p < n; ++p) out.push_back(llm_block_cost(&model, p));
  llm_cost_model_free(&model);
  return out;
}

void test_cost_models() {
  printf("cost models\n");
  using V = std::vector<int64_t>;
  CHECK_EQ(costs(nullptr, 3) == V({1, 1, 1}), true);
  CHECK_EQ(costs("uniform", 3) == V({1, 1, 1}), true);
  CHECK_EQ(costs("position", 3) == V({1, 2, 3}), true);
  CHECK_EQ(costs("qwen3coder30b_blksz_16", 3) == V({865, 867, 869}), true);
  CHECK_EQ(costs("linear:865,2", 3) == V({865, 867, 869}), true);
  CHECK_EQ(costs("linear: 10 , 0", 2) == V({10, 10}), true);

  // positions past the table's end cost what its last line does
  const char *table = "test_lcp_reader_tmp_cost.txt";
  std::ofstream(table) << "# measured\n5\n\n  7\n9\n";
  const std::string spec = std::string("table:") + table;
  CHECK_EQ(costs(spec.c_str(), 5) == V({5, 7, 9, 9, 9}), true);

  // every block must cost at least 1, in integers that fit cache_obj_t
  for (const char *bad :
       {"linear:0,1", "linear:1,-1", "linear:1.5,2", "linear:1", "linear:,2",
        "linear:1,2,3", "linear:2147483648,0", "table:/nonexistent/costs.txt",
        "cubic", ""}) {
    if (costs(bad, 1) != V({-1})) {
      printf("  FAIL: cost model '%s' parsed\n", bad);
      ++n_failed;
    }
  }
  std::ofstream("test_lcp_reader_tmp_bad.txt") << "4\n0\n";
  CHECK_EQ(costs("table:test_lcp_reader_tmp_bad.txt", 1) == V({-1}), true);
  std::ofstream("test_lcp_reader_tmp_bad.txt") << "# nothing\n\n";
  CHECK_EQ(costs("table:test_lcp_reader_tmp_bad.txt", 1) == V({-1}), true);
  remove("test_lcp_reader_tmp_bad.txt");

  llm_cost_model_t m;
  char err[1024];
  CHECK_EQ(llm_cost_model_parse("linear:3,0", &m, err, sizeof(err)), true);
  CHECK_EQ(llm_cost_model_is_uniform(&m), true);
  CHECK_EQ(llm_cost_model_max(&m, 4), 3);
  CHECK_EQ(llm_cost_model_parse("linear:3,2", &m, err, sizeof(err)), true);
  CHECK_EQ(llm_cost_model_is_uniform(&m), false);
  CHECK_EQ(llm_cost_model_max(&m, 4), 9);
  CHECK_EQ(llm_cost_model_parse(spec.c_str(), &m, err, sizeof(err)), true);
  CHECK_EQ(llm_cost_model_is_uniform(&m), false);
  CHECK_EQ(llm_cost_model_max(&m, 2), 7);
  llm_cost_model_free(&m);

  // the flat reader derives obj_cost from the model it is given
  const char *path = "test_lcp_reader_tmp.lcp";
  write_trace(path);
  reader_init_param_t params;
  set_default_reader_init_params(&params);
  params.llm_cost_model = const_cast<char *>(spec.c_str());
  reader_t *reader = setup_reader(path, LCP_TRACE, &params);
  request_t *req = new_request();
  std::vector<int64_t> got;
  while (read_one_req(reader, req) == 0) got.push_back(req->obj_cost);
  CHECK_EQ(got == V({5, 7, 9, 5, 7, 9}), true);
  free_request(req);
  close_reader(reader);
  remove(path);
  remove(table);
}

/// An object reached with a new cost takes it on rather than aborting: a cost
/// column may vary over time, as may a raw LLM block id reached at a new
/// position.
void test_cost_change() {
  printf("cost change on a hit\n");
  common_cache_params_t cache_params = default_common_cache_params();
  cache_params.cache_size = 4;
  cache_params.consider_obj_metadata = false;
  cache_t *cache = LRU_init(cache_params, nullptr);
  request_t *req = new_request();
  req->obj_id = 42;
  req->obj_size = 1;
  req->obj_cost = 5;
  cache->get(cache, req);
  req->obj_cost = 7;
  CHECK_EQ(cache->get(cache, req), true);
  const cache_obj_t *obj = cache->find(cache, req, false);
  CHECK_EQ(obj != nullptr ? obj->cost : -1, 7);
  free_request(req);
  cache->cache_free(cache);
}

}  // namespace

int main(void) {
  test_flat_reader();
  test_cost_models();
  test_cost_change();
  if (n_failed != 0) {
    printf("%d check(s) FAILED\n", n_failed);
    return 1;
  }
  printf("all lcp reader checks passed\n");
  return 0;
}

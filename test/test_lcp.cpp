// Checks for the lcp trace format, on traces small enough to derive by hand:
//   - prefixsim's JSONL reader, which assigns the ids the converter writes:
//     positional encoding, dense first-appearance numbering, and the request
//     metadata the format carries
//   - libCacheSim's flat lcp reader, on a file built here directly from the
//     spec's structs: block ids, derived cost, next access, timestamp in
//     seconds, and rewinding mid-request
//   - prefixsim's lcp reader on the same file: whole requests, with block
//     and session next access as stored
//   - the round trip JSONL -> lcp -> prefixsim: every field of every request
//     comes back as the JSONL reader produced it, and simulating the stream in
//     one pass matches simulating the JSONL loaded whole

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "lcp_writer.hpp"
#include "simulator.hpp"
#include "trace.hpp"

extern "C" {
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

std::vector<prefixsim::Request> load_jsonl(const std::string &body,
                                           prefixsim::BlockIdMode mode) {
  const char *path = "test_lcp_tmp.jsonl";
  std::ofstream(path) << body;
  std::string error;
  std::vector<prefixsim::Request> requests;
  auto reader = prefixsim::open_trace(path, prefixsim::TraceFormat::kQwenJsonl,
                                      mode, error);
  if (reader == nullptr || !prefixsim::load_trace(*reader, requests, error)) {
    printf("  FAIL: %s\n", error.c_str());
    ++n_failed;
  }
  remove(path);
  return requests;
}

const std::string kJsonl =
    // raw 20 under raw 10; raw 30 deepest
    "{\"chat_id\":5,\"parent_chat_id\":-1,\"timestamp\":1.5,"
    "\"timestamp_arrival\":1.25,\"user_id\":\"u1\","
    "\"input_length\":48,\"output_length\":null,\"type\":\"text\","
    "\"turn\":1,\"session_id\":\"a\",\"hash_ids\":[10,20,30]}\n"
    // shares the prefix 10,20, then diverges
    "{\"chat_id\":6,\"parent_chat_id\":5,\"timestamp\":2.0,"
    "\"timestamp_arrival\":1.75,\"user_id\":\"u2\","
    "\"type\":\"text\",\"turn\":2,\"session_id\":\"b\","
    "\"hash_ids\":[10,20,31]}\n"
    // no blocks: skipped, so it must not claim session id 3 or user id 3
    "{\"timestamp\":2.5,\"session_id\":\"skipped\",\"user_id\":\"skipped\","
    "\"hash_ids\":[]}\n"
    // raw 20 again, but under a different prefix: a different block. No
    // type or turn: category 0. No user. The arrival time carries the float
    // noise of `completion - latency`, which the file rounds to the us
    "{\"timestamp\":3.0,\"timestamp_arrival\":2.999999999,"
    "\"session_id\":\"c\",\"hash_ids\":[99,20]}\n"
    // the first request's path again, same session. No arrival time
    "{\"timestamp\":1752192000.123456,\"type\":\"text\",\"turn\":1,"
    "\"session_id\":\"a\",\"user_id\":\"u3\","
    "\"hash_ids\":[10,20,30]}\n";

void test_jsonl_ids() {
  printf("JSONL reader: positional encoding and interning\n");
  const auto r = load_jsonl(kJsonl, prefixsim::BlockIdMode::kPrefix);
  CHECK_EQ(r.size(), 4);
  if (r.size() != 4) return;
  const std::vector<std::vector<obj_id_t>> want = {
      {1, 2, 3}, {1, 2, 4}, {5, 6}, {1, 2, 3}};
  for (size_t i = 0; i < want.size(); ++i) {
    CHECK_EQ(r[i].blocks.size(), want[i].size());
    for (size_t j = 0; j < want[i].size() && j < r[i].blocks.size(); ++j) {
      CHECK_EQ(r[i].blocks[j], want[i][j]);
    }
  }
  CHECK_EQ(r[0].session, 1);
  CHECK_EQ(r[1].session, 2);
  CHECK_EQ(r[2].session, 3);  // "skipped" took no id
  CHECK_EQ(r[3].session, 1);
  CHECK_EQ(r[0].user, 1);
  CHECK_EQ(r[1].user, 2);
  CHECK_EQ(r[2].user, 0);  // absent
  CHECK_EQ(r[3].user, 3);  // "skipped" took no id
  CHECK_EQ(r[0].start_timestamp == 1.25, true);
  // absent, so taken to be the completion, as in the Qwen traces
  CHECK_EQ(r[3].start_timestamp == r[3].timestamp, true);
  CHECK_EQ(r[0].category, 1);  // (text, 1)
  CHECK_EQ(r[1].category, 2);  // (text, 2)
  CHECK_EQ(r[2].category, 0);  // neither field
  CHECK_EQ(r[3].category, 1);
  CHECK_EQ(r[0].chat_id, 5);
  CHECK_EQ(r[1].parent_chat_id, 5);
  CHECK_EQ(r[2].chat_id, -1);  // absent
  CHECK_EQ(r[0].input_length, 48);
  CHECK_EQ(r[0].output_length, -1);  // null
  CHECK_EQ(r[1].turn, 2);
  CHECK_EQ(r[2].type.empty(), true);

  // raw mode keeps the trace's ids, and so merges the two raw-20 blocks
  const auto raw = load_jsonl(kJsonl, prefixsim::BlockIdMode::kRaw);
  CHECK_EQ(raw.size(), 4);
  if (raw.size() == 4) {
    CHECK_EQ(raw[0].blocks[1], 20);
    CHECK_EQ(raw[2].blocks[1], 20);
  }
}

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
  // forward vtimes 1..6: id 1 recurs at 4, id 2 at 5, nothing else recurs
  const int64_t next[2][3] = {{4, 5, -1}, {-1, -1, -1}};
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
      const lcp_block_t b = {ids[r][i], next[r][i]};
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
  const char *path = "test_lcp_tmp.lcp";
  write_trace(path);

  reader_init_param_t params;
  set_default_reader_init_params(&params);
  params.llm_cost_model = LLM_COST_POSITION;
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

void test_prefixsim_reader() {
  printf("prefixsim lcp reader\n");
  const char *path = "test_lcp_tmp.lcp";
  write_trace(path);

  std::string error;
  auto reader = prefixsim::open_trace(path, prefixsim::TraceFormat::kLcp,
                                      prefixsim::BlockIdMode::kPrefix, error);
  CHECK_EQ(reader != nullptr, true);
  if (reader != nullptr) {
    CHECK_EQ(reader->annotated(), true);
    CHECK_EQ(reader->n_requests_total(), 2);
    CHECK_EQ(reader->n_blocks_total(), 6);

    prefixsim::Request r;
    int n_req = 0, n_block = 0;
    while (reader->next(r)) {
      CHECK_EQ(r.index, n_req);
      CHECK_EQ(r.session, 1);
      CHECK_EQ(r.category, 0);  // neither type nor turn
      CHECK_EQ(r.blocks.size(), 3);
      CHECK_EQ(r.next_access_vtime.size(), r.blocks.size());
      for (size_t i = 0; i < r.blocks.size() && n_block < 6; ++i, ++n_block) {
        CHECK_EQ(r.blocks[i], kWantId[n_block]);
        CHECK_EQ(r.next_access_vtime[i], kWantNext[n_block]);
      }
      ++n_req;
    }
    CHECK_EQ(n_req, 2);
  }

  // re-read whole, to see the per-request fields side by side
  std::vector<prefixsim::Request> rs;
  reader = prefixsim::open_trace(path, prefixsim::TraceFormat::kLcp,
                                 prefixsim::BlockIdMode::kPrefix, error);
  if (reader != nullptr && prefixsim::load_trace(*reader, rs, error) &&
      rs.size() == 2) {
    CHECK_EQ(rs[0].timestamp == 1.5, true);
    CHECK_EQ(rs[1].timestamp == 2.7, true);
    CHECK_EQ(rs[0].start_timestamp == 1.0, true);
    CHECK_EQ(rs[1].start_timestamp == 2.2, true);
    CHECK_EQ(rs[1].user, 1);
    CHECK_EQ(rs[0].session_next_access, 1);  // 0-based in Request
    CHECK_EQ(rs[1].session_next_access, INT64_MAX);
    CHECK_EQ(rs[0].chat_id, -1);
    CHECK_EQ(rs[0].type.empty(), true);
  } else {
    printf("  FAIL: reloading: %s\n", error.c_str());
    ++n_failed;
  }

  // raw ids make no sense for a format whose ids are positional already
  CHECK_EQ(
      prefixsim::open_trace(path, prefixsim::TraceFormat::kLcp,
                            prefixsim::BlockIdMode::kRaw, error) == nullptr,
      true);
  remove(path);
}

/// The file keeps times to the microsecond. Completion times in the fixture
/// have at most 6 decimals and must survive exactly; arrival times are
/// compared at the precision the file promises.
bool same_us(double a, double b) {
  return std::llround(a * 1e6) == std::llround(b * 1e6);
}

bool same_request(const prefixsim::Request &a, const prefixsim::Request &b) {
  return a.index == b.index && a.timestamp == b.timestamp &&
         same_us(a.start_timestamp, b.start_timestamp) && a.user == b.user &&
         a.blocks == b.blocks && a.session == b.session &&
         a.session_next_access == b.session_next_access &&
         a.category == b.category && a.chat_id == b.chat_id &&
         a.parent_chat_id == b.parent_chat_id &&
         a.input_length == b.input_length &&
         a.output_length == b.output_length && a.turn == b.turn &&
         a.type == b.type && a.next_access_vtime == b.next_access_vtime;
}

/// Run `algorithm` once over the JSONL loaded whole, once over the lcp
/// stream, and compare.
void check_same_simulation(const std::vector<prefixsim::Request> &requests,
                           const char *lcp_path, const char *algorithm,
                           int64_t cache_size) {
  prefixsim::SimulatorConfig config;
  config.cache_size_blocks = cache_size;
  config.cost_model = prefixsim::CostModel::kPosition;

  prefixsim::Stats loaded, streamed;
  std::string error;
  for (int pass = 0; pass < 2; ++pass) {
    cache_t *cache = prefixsim::create_cache_by_name(algorithm, cache_size);
    bool ok = false;
    {
      prefixsim::Simulator sim(cache, config);
      if (pass == 0) {
        ok = sim.run(requests, error);
        loaded = sim.stats();
      } else {
        auto reader =
            prefixsim::open_trace(lcp_path, prefixsim::TraceFormat::kLcp,
                                  prefixsim::BlockIdMode::kPrefix, error);
        ok = reader != nullptr && sim.run(*reader, error);
        streamed = sim.stats();
      }
    }
    cache->cache_free(cache);
    if (!ok) {
      printf("  FAIL [%s]: %s\n", algorithm, error.c_str());
      ++n_failed;
      return;
    }
  }
  CHECK_EQ(streamed.n_blocks, loaded.n_blocks);
  CHECK_EQ(streamed.n_block_hits, loaded.n_block_hits);
  CHECK_EQ(streamed.saved_cost == loaded.saved_cost, true);
  CHECK_EQ(streamed.n_evictions, loaded.n_evictions);
}

void test_round_trip() {
  printf("round trip: JSONL -> lcp -> prefixsim\n");
  const char *path = "test_lcp_tmp_rt.lcp";
  const auto jsonl = load_jsonl(kJsonl, prefixsim::BlockIdMode::kPrefix);

  lcp_trace_stat_t stat;
  std::string error;
  if (!prefixsim::write_lcp(jsonl, 16, path, stat, error)) {
    printf("  FAIL: write: %s\n", error.c_str());
    ++n_failed;
    return;
  }
  CHECK_EQ(stat.n_req, 4);
  CHECK_EQ(stat.n_block, 11);
  CHECK_EQ(stat.n_unique_block, 6);
  CHECK_EQ(stat.n_session, 3);
  CHECK_EQ(stat.n_user, 3);
  CHECK_EQ(stat.n_type, 1);

  std::vector<prefixsim::Request> back;
  auto reader = prefixsim::open_trace(path, prefixsim::TraceFormat::kLcp,
                                      prefixsim::BlockIdMode::kPrefix, error);
  if (reader == nullptr || !prefixsim::load_trace(*reader, back, error)) {
    printf("  FAIL: read back: %s\n", error.c_str());
    ++n_failed;
    remove(path);
    return;
  }
  CHECK_EQ(back.size(), jsonl.size());
  for (size_t i = 0; i < back.size() && i < jsonl.size(); ++i) {
    if (!same_request(back[i], jsonl[i])) {
      printf("  FAIL: request %zu differs after the round trip\n", i);
      ++n_failed;
    }
  }
  if (back.size() == 4) {
    // the float noise in 2.999999999 is rounded away
    CHECK_EQ(back[2].start_timestamp == 3.0, true);
    CHECK_EQ(back[3].start_timestamp == back[3].timestamp, true);
  }

  // every source of future knowledge a policy can use, and a plain baseline
  for (const char *algorithm : {"lru", "belady_compute", "session_belady"}) {
    check_same_simulation(jsonl, path, algorithm, 4);
  }
  remove(path);
}

}  // namespace

int main(void) {
  test_jsonl_ids();
  test_flat_reader();
  test_prefixsim_reader();
  test_round_trip();
  if (n_failed != 0) {
    printf("%d check(s) FAILED\n", n_failed);
    return 1;
  }
  printf("all lcp checks passed\n");
  return 0;
}

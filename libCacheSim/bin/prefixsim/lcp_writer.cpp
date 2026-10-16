#include "lcp_writer.hpp"

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace prefixsim {
namespace {

int64_t to_us(double seconds) {
  return static_cast<int64_t>(std::llround(seconds * 1e6));
}

/// Check what the format promises about request order and time, and build the
/// stat block and the type table. Returns false with `error` set if the trace
/// cannot be written as lcp.
bool build_stat(const std::vector<Request> &requests, int32_t block_size_tokens,
                lcp_trace_stat_t &stat, std::vector<int32_t> &type_ids,
                std::string &error) {
  memset(&stat, 0, sizeof(stat));
  stat.version = LCP_CURR_STAT_VERSION;
  stat.n_req = static_cast<int64_t>(requests.size());
  stat.block_size_tokens = block_size_tokens;
  stat.start_timestamp_us = to_us(requests.front().timestamp);
  stat.end_timestamp_us = to_us(requests.back().timestamp);

  std::unordered_map<std::string, int32_t> type_index;
  type_ids.reserve(requests.size());
  uint64_t max_block = 0, max_session = 0;
  bool any_nonzero_time = false;
  char buf[256];

  for (size_t i = 0; i < requests.size(); ++i) {
    const Request &r = requests[i];

    // Wall-clock policies assume time only moves forward.
    if (i > 0 && r.timestamp < requests[i - 1].timestamp) {
      snprintf(buf, sizeof(buf),
               "request %zu has timestamp %.6f, earlier than request %zu's "
               "%.6f; lcp requires requests in timestamp order",
               i, r.timestamp, i - 1, requests[i - 1].timestamp);
      error = buf;
      return false;
    }
    if (r.timestamp != 0.0) any_nonzero_time = true;

    if (r.blocks.size() > UINT32_MAX) {
      snprintf(buf, sizeof(buf), "request %zu has %zu blocks, over 2^32", i,
               r.blocks.size());
      error = buf;
      return false;
    }
    if (r.next_access_vtime.size() != r.blocks.size()) {
      error = "internal: requests are not annotated with next access";
      return false;
    }
    const int64_t n = static_cast<int64_t>(r.blocks.size());
    stat.n_block += n;
    if (n > stat.max_blocks_per_req) stat.max_blocks_per_req = n;

    // The reader guarantees ids 1, 2, 3, ... in order of first appearance;
    // a gap here would be a bug in it, not a property of the trace.
    for (const obj_id_t id : r.blocks) {
      if (id == 0 || id > max_block + 1) {
        snprintf(buf, sizeof(buf),
                 "internal: block id %llu in request %zu skips past %llu",
                 (unsigned long long)id, i, (unsigned long long)max_block);
        error = buf;
        return false;
      }
      if (id > max_block) max_block = id;
    }
    if (r.session > max_session) max_session = r.session;

    int32_t type_id = -1;
    if (!r.type.empty()) {
      auto it = type_index.find(r.type);
      if (it == type_index.end()) {
        if (type_index.size() == LCP_MAX_TYPE ||
            r.type.size() >= LCP_TYPE_NAME_LEN) {
          snprintf(buf, sizeof(buf),
                   "request type '%.64s' does not fit the header's table "
                   "(at most %d types of under %d bytes)",
                   r.type.c_str(), LCP_MAX_TYPE, LCP_TYPE_NAME_LEN);
          error = buf;
          return false;
        }
        const int32_t id = static_cast<int32_t>(type_index.size());
        it = type_index.emplace(r.type, id).first;
        memcpy(stat.type_names[id], r.type.data(), r.type.size());
      }
      type_id = it->second;
    }
    type_ids.push_back(type_id);
  }

  // A trace whose timestamps all parse as 0 almost certainly stores them in a
  // form the reader does not understand (e.g. ISO-8601 strings).
  if (requests.size() > 1 && !any_nonzero_time) {
    error =
        "every timestamp is 0: the trace has none, or stores them in a form "
        "the reader cannot parse (it expects seconds as a number)";
    return false;
  }
  if (max_session > UINT32_MAX) {
    error = "more than 2^32 sessions";
    return false;
  }

  stat.n_unique_block = static_cast<int64_t>(max_block);
  stat.n_session = static_cast<int64_t>(max_session);
  stat.n_type = static_cast<int64_t>(type_index.size());
  return true;
}

}  // namespace

bool write_lcp(const std::vector<Request> &requests, int32_t block_size_tokens,
               const std::string &path, lcp_trace_stat_t &stat,
               std::string &error) {
  if (requests.empty()) {
    error = "no requests to write";
    return false;
  }

  lcp_trace_header_t header;
  memset(&header, 0, sizeof(header));
  header.start_magic = LCP_TRACE_MAGIC;
  header.end_magic = LCP_TRACE_MAGIC;
  header.version = LCP_CURR_VERSION;
  std::vector<int32_t> type_ids;
  if (!build_stat(requests, block_size_tokens, header.stat, type_ids, error)) {
    return false;
  }
  stat = header.stat;

  FILE *out = fopen(path.c_str(), "wb");
  if (out == nullptr) {
    error = "cannot open '" + path + "' for writing";
    return false;
  }
  bool ok = fwrite(&header, sizeof(header), 1, out) == 1;

  std::vector<lcp_block_t> blocks;
  for (size_t i = 0; ok && i < requests.size(); ++i) {
    const Request &r = requests[i];
    lcp_req_v1_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.timestamp_us = to_us(r.timestamp);
    rec.chat_id = r.chat_id;
    rec.parent_chat_id = r.parent_chat_id;
    // Request holds a 0-based index; the file a 1-based one, as for blocks
    rec.session_next_req =
        r.session_next_access == INT64_MAX ? -1 : r.session_next_access + 1;
    rec.session_id = static_cast<uint32_t>(r.session);
    rec.n_blocks = static_cast<uint32_t>(r.blocks.size());
    rec.input_length = r.input_length;
    rec.output_length = r.output_length;
    rec.turn = r.turn;
    rec.type_id = type_ids[i];
    ok = fwrite(&rec, sizeof(rec), 1, out) == 1;

    const size_t n = r.blocks.size();
    blocks.resize(n);
    for (size_t j = 0; j < n; ++j) {
      blocks[j].block_id = r.blocks[j];
      // load_trace uses MAX_REUSE_DISTANCE for "never"; the format uses -1.
      blocks[j].next_access_vtime = r.next_access_vtime[j] >= MAX_REUSE_DISTANCE
                                        ? -1
                                        : r.next_access_vtime[j];
    }
    ok = ok && fwrite(blocks.data(), sizeof(lcp_block_t), n, out) == n;
  }

  if (fclose(out) != 0) ok = false;
  if (!ok) {
    error = "writing '" + path + "' failed";
    return false;
  }
  return true;
}

}  // namespace prefixsim

#include "trace.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unordered_map>

extern "C" {
#include "libCacheSim/const.h"
}

namespace prefixsim {
namespace {

constexpr uint64_t kGoldenRatio = 0x9E3779B9ULL;

/// boost::hash_combine, 64-bit. Byte-for-byte the same as hash_combine_u64() in
/// llm_cachesim/scripts/trace_processing/llm_qwen.py, so a trace converted to
/// .lcsllm and the same trace read natively here produce the same identities.
inline uint64_t hash_combine_u64(uint64_t seed, uint64_t value) {
  return seed ^ (value + kGoldenRatio + (seed << 6) + (seed >> 2));
}

/// Locate the value of `"key":` in one JSON line and return a pointer just past
/// the colon, or nullptr when the key is absent. This is a field scanner, not a
/// JSON parser: it assumes the machine-generated, one-object-per-line schema
/// the LLM traces use, and it is roughly an order of magnitude faster than a
/// general parser on the multi-GB traces.
const char *find_field(const char *line, const char *quoted_key) {
  const char *p = strstr(line, quoted_key);
  if (p == nullptr) return nullptr;
  p += strlen(quoted_key);
  while (*p == ' ' || *p == '\t') ++p;
  if (*p != ':') return nullptr;
  ++p;
  while (*p == ' ' || *p == '\t') ++p;
  return p;
}

/// JSONL with one request per line and a `hash_ids` array of block ids.
class QwenJsonlReader : public TraceReader {
 public:
  QwenJsonlReader(std::ifstream in, BlockIdMode mode) : in_(std::move(in)), mode_(mode) {}

  bool next(Request &out) override {
    while (std::getline(in_, line_)) {
      if (line_.empty()) continue;

      const char *ids = find_field(line_.c_str(), "\"hash_ids\"");
      if (ids == nullptr || *ids != '[') continue;
      ++ids;  // step over '['

      out.blocks.clear();
      out.next_access_vtime.clear();
      out.index = n_emitted_;
      out.timestamp = 0.0;

      const char *ts = find_field(line_.c_str(), "\"timestamp\"");
      if (ts != nullptr) out.timestamp = strtod(ts, nullptr);

      uint64_t rolling = 0;
      bool first = true;
      while (*ids != '\0' && *ids != ']') {
        while (*ids == ' ' || *ids == ',') ++ids;
        if (*ids == ']' || *ids == '\0') break;

        char *end = nullptr;
        const uint64_t raw = strtoull(ids, &end, 10);
        if (end == ids) break;  // malformed element; stop this line here
        ids = end;

        if (mode_ == BlockIdMode::kRaw) {
          out.blocks.push_back(static_cast<obj_id_t>(raw));
        } else {
          rolling = first ? raw : hash_combine_u64(rolling, raw);
          out.blocks.push_back(static_cast<obj_id_t>(rolling));
        }
        first = false;
      }

      if (out.blocks.empty()) continue;
      ++n_emitted_;
      return true;
    }
    return false;
  }

  const char *format_name() const override { return "qwen-jsonl"; }

 private:
  std::ifstream in_;
  BlockIdMode mode_;
  std::string line_;
  int64_t n_emitted_ = 0;
};

}  // namespace

bool parse_trace_format(const std::string &name, TraceFormat &out) {
  if (name == "qwen-jsonl" || name == "qwen") {
    out = TraceFormat::kQwenJsonl;
    return true;
  }
  return false;
}

const char *trace_format_name(TraceFormat format) {
  switch (format) {
    case TraceFormat::kQwenJsonl:
      return "qwen-jsonl";
  }
  return "?";
}

std::unique_ptr<TraceReader> open_trace(const std::string &path, TraceFormat format,
                                        BlockIdMode mode, std::string &error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error = "cannot open trace file: " + path;
    return nullptr;
  }
  switch (format) {
    case TraceFormat::kQwenJsonl:
      return std::unique_ptr<TraceReader>(new QwenJsonlReader(std::move(in), mode));
  }
  error = "unhandled trace format";
  return nullptr;
}

bool load_trace(TraceReader &reader, std::vector<Request> &out, std::string &error) {
  Request req;
  while (reader.next(req)) {
    out.push_back(req);
  }
  if (out.empty()) {
    error = "trace contained no usable requests";
    return false;
  }
  annotate_next_access(out);
  return true;
}

void annotate_next_access(std::vector<Request> &requests) {
  int64_t total = 0;
  for (Request &r : requests) {
    r.next_access_vtime.assign(r.blocks.size(), MAX_REUSE_DISTANCE);
    total += static_cast<int64_t>(r.blocks.size());
  }

  // One reverse pass. Walking backwards, "the most recent vtime at which I saw
  // this id" is exactly the *next* access of the block we are standing on.
  std::unordered_map<obj_id_t, int64_t> next_seen;
  next_seen.reserve(static_cast<size_t>(total / 4 + 1));

  int64_t vtime = total;  // 1-based vtime of the last block in forward order
  for (auto ri = requests.rbegin(); ri != requests.rend(); ++ri) {
    for (int64_t i = static_cast<int64_t>(ri->blocks.size()) - 1; i >= 0; --i) {
      const size_t idx = static_cast<size_t>(i);
      const obj_id_t id = ri->blocks[idx];
      auto it = next_seen.find(id);
      if (it != next_seen.end()) {
        ri->next_access_vtime[idx] = it->second;
      }
      next_seen[id] = vtime;
      --vtime;
    }
  }
}

}  // namespace prefixsim

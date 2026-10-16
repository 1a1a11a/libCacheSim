#include "trace.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unordered_map>

extern "C" {
#include "libCacheSim/const.h"
#include "libCacheSim/reader.h"
#include "traceReader/customizedReader/lcp.h"
}

namespace prefixsim {
namespace {

/// splitmix64's finaliser. Only spreads keys across hash-table buckets; no id
/// is ever derived from it, so its collisions cost speed, never correctness.
inline uint64_t mix64(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

/// A block reached through a given prefix: the interned id of its parent (0
/// for a prefix root) and its raw id in the trace.
struct PathKey {
  uint64_t parent;
  uint64_t raw;
  bool operator==(const PathKey &o) const {
    return parent == o.parent && raw == o.raw;
  }
};

struct PathKeyHash {
  size_t operator()(const PathKey &k) const noexcept {
    return static_cast<size_t>(mix64(k.parent * 0x9E3779B97F4A7C15ULL ^ k.raw));
  }
};

/// Map each distinct key to 1, 2, 3, ... in order of first appearance. Exact
/// where a hash would only be probable: two keys share an id only when equal.
template <typename Key, typename Hash = std::hash<Key>>
class Interner {
 public:
  uint64_t intern(const Key &key) {
    auto it = ids_.emplace(key, ids_.size() + 1).first;
    return it->second;
  }
  void release() { std::unordered_map<Key, uint64_t, Hash>().swap(ids_); }

 private:
  std::unordered_map<Key, uint64_t, Hash> ids_;
};

/// arXiv:2506.02634's request category: the (request type, turn number) pair,
/// interned to 1, 2, 3, ... in order of first appearance. 0 is reserved for
/// "neither field", which policies read as one class. Every reader goes
/// through this, so a trace has the same categories in every format.
class CategoryInterner {
 public:
  /// An empty `type` means the trace has none; a negative `turn` likewise.
  uint64_t intern(const std::string &type, int32_t turn) {
    if (type.empty() && turn < 0) return 0;
    std::string key = type.empty() ? "-" : "t" + type;
    key += '\x1f';
    key += turn >= 0 ? std::to_string(turn) : "-";
    return ids_.intern(key);
  }
  void release() { ids_.release(); }

 private:
  Interner<std::string> ids_;
};

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

/// Integer value of `"key":`, or `absent` when the key is missing, null, or
/// not a number.
int64_t int_field(const char *line, const char *quoted_key, int64_t absent) {
  const char *p = find_field(line, quoted_key);
  if (p == nullptr || !(*p == '-' || (*p >= '0' && *p <= '9'))) return absent;
  return strtoll(p, nullptr, 10);
}

/// String value of `"key":` into `out`; false when missing or not a string.
bool string_field(const char *line, const char *quoted_key, std::string &out) {
  const char *p = find_field(line, quoted_key);
  if (p == nullptr || *p != '"') return false;
  ++p;  // step over the opening quote
  const char *end = strchr(p, '"');
  if (end == nullptr) return false;
  out.assign(p, static_cast<size_t>(end - p));
  return true;
}

/// JSONL with one request per line and a `hash_ids` array of block ids.
class QwenJsonlReader : public TraceReader {
 public:
  QwenJsonlReader(std::ifstream in, BlockIdMode mode)
      : in_(std::move(in)), mode_(mode) {}

  bool next(Request &out) override {
    while (std::getline(in_, line_)) {
      if (line_.empty()) continue;
      const char *line = line_.c_str();

      const char *ids = find_field(line, "\"hash_ids\"");
      if (ids == nullptr || *ids != '[') continue;
      ++ids;  // step over '['

      out.blocks.clear();
      out.next_access_vtime.clear();
      out.index = n_emitted_;
      out.timestamp = 0.0;

      uint64_t parent = 0;  // 0 = prefix root; interned ids start at 1
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
          parent = blocks_.intern(PathKey{parent, raw});
          out.blocks.push_back(static_cast<obj_id_t>(parent));
        }
      }

      if (out.blocks.empty()) continue;

      // Only now, so a skipped line cannot claim a session or category id and
      // leave a gap in the 1, 2, 3, ... numbering.
      const char *ts = find_field(line, "\"timestamp\"");
      if (ts != nullptr) out.timestamp = strtod(ts, nullptr);

      out.chat_id = int_field(line, "\"chat_id\"", -1);
      out.parent_chat_id = int_field(line, "\"parent_chat_id\"", -1);
      out.input_length =
          static_cast<int32_t>(int_field(line, "\"input_length\"", -1));
      out.output_length =
          static_cast<int32_t>(int_field(line, "\"output_length\"", -1));
      out.turn = static_cast<int32_t>(int_field(line, "\"turn\"", -1));
      // "type": "" counts as no type, as it does in lcp
      if (!string_field(line, "\"type\"", out.type)) out.type.clear();
      out.category = categories_.intern(out.type, out.turn);

      std::string session;
      out.session = string_field(line, "\"session_id\"", session)
                        ? sessions_.intern(session)
                        : 0;

      ++n_emitted_;
      return true;
    }
    // End of trace: the interning tables are not needed any more and can be
    // as large as the next-access table built after loading.
    blocks_.release();
    sessions_.release();
    categories_.release();
    return false;
  }

  const char *format_name() const override { return "qwen-jsonl"; }

 private:
  std::ifstream in_;
  BlockIdMode mode_;
  std::string line_;
  int64_t n_emitted_ = 0;
  Interner<PathKey, PathKeyHash> blocks_;
  Interner<std::string> sessions_;
  CategoryInterner categories_;
};

/// lcp, read a request at a time through libCacheSim's own reader, which
/// brings zstd and the header and integrity checks with it. The file stores
/// next access for blocks and sessions, so requests come out annotated and
/// the trace never has to be held in memory.
class LcpReader : public TraceReader {
 public:
  explicit LcpReader(reader_t *reader)
      : reader_(reader), stat_(*lcp_get_stat(reader)) {
    entries_.resize(static_cast<size_t>(stat_.max_blocks_per_req));
  }
  ~LcpReader() override { close_reader(reader_); }

  bool next(Request &out) override {
    lcp_req_v1_t rec;
    if (lcp_read_trace_req(reader_, &rec, entries_.data()) != 0) {
      return false;
    }
    const size_t n = rec.n_blocks;
    out.blocks.resize(n);
    out.next_access_vtime.resize(n);
    for (size_t i = 0; i < n; ++i) {
      out.blocks[i] = static_cast<obj_id_t>(entries_[i].block_id);
      out.next_access_vtime[i] = entries_[i].next_access_vtime < 0
                                     ? MAX_REUSE_DISTANCE
                                     : entries_[i].next_access_vtime;
    }
    out.index = n_emitted_++;
    // Exact for timestamps of up to 6 decimals: the integer divides to the
    // double nearest the decimal, which is what strtod gives the JSONL.
    out.timestamp = static_cast<double>(rec.timestamp_us) / 1e6;
    out.session = rec.session_id;
    // the file is 1-based with -1 for none; Request uses 0-based indices
    out.session_next_access =
        rec.session_next_req < 0 ? INT64_MAX : rec.session_next_req - 1;
    out.chat_id = rec.chat_id;
    out.parent_chat_id = rec.parent_chat_id;
    out.input_length = rec.input_length;
    out.output_length = rec.output_length;
    out.turn = rec.turn;
    if (rec.type_id >= 0) {
      const char *name = stat_.type_names[rec.type_id];
      out.type.assign(name, strnlen(name, LCP_TYPE_NAME_LEN));
    } else {
      out.type.clear();
    }
    out.category = categories_.intern(out.type, out.turn);
    return true;
  }

  const char *format_name() const override { return "lcp"; }
  bool annotated() const override { return true; }
  int64_t n_requests_total() const override { return stat_.n_req; }
  int64_t n_blocks_total() const override { return stat_.n_block; }

 private:
  reader_t *reader_;
  lcp_trace_stat_t stat_;
  std::vector<lcp_block_t> entries_;
  int64_t n_emitted_ = 0;
  CategoryInterner categories_;
};

}  // namespace

bool parse_trace_format(const std::string &name, TraceFormat &out) {
  if (name == "qwen-jsonl" || name == "qwen") {
    out = TraceFormat::kQwenJsonl;
    return true;
  }
  if (name == "lcp") {
    out = TraceFormat::kLcp;
    return true;
  }
  return false;
}

TraceFormat guess_trace_format(const std::string &path) {
  return path.find(".lcp") != std::string::npos ? TraceFormat::kLcp
                                                : TraceFormat::kQwenJsonl;
}

const char *trace_format_name(TraceFormat format) {
  switch (format) {
    case TraceFormat::kQwenJsonl:
      return "qwen-jsonl";
    case TraceFormat::kLcp:
      return "lcp";
  }
  return "?";
}

std::unique_ptr<TraceReader> open_trace(const std::string &path,
                                        TraceFormat format, BlockIdMode mode,
                                        std::string &error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error = "cannot open trace file: " + path;
    return nullptr;
  }
  switch (format) {
    case TraceFormat::kQwenJsonl:
      return std::unique_ptr<TraceReader>(
          new QwenJsonlReader(std::move(in), mode));
    case TraceFormat::kLcp: {
      if (mode == BlockIdMode::kRaw) {
        error =
            "--block-id raw does not apply to lcp: its ids are already "
            "positionally encoded";
        return nullptr;
      }
      in.close();
      reader_init_param_t params;
      set_default_reader_init_params(&params);
      // exits with an ERROR on a file that is not a valid lcp trace
      reader_t *reader = setup_reader(path.c_str(), LCP_TRACE, &params);
      return std::unique_ptr<TraceReader>(new LcpReader(reader));
    }
  }
  error = "unhandled trace format";
  return nullptr;
}

bool load_trace(TraceReader &reader, std::vector<Request> &out,
                std::string &error) {
  Request req;
  while (reader.next(req)) {
    out.push_back(req);
  }
  if (out.empty()) {
    error = "trace contained no usable requests";
    return false;
  }
  if (!reader.annotated()) {
    annotate_next_access(out);
    annotate_session_next_access(out);
  }
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

void annotate_session_next_access(std::vector<Request> &requests) {
  // One reverse pass, same trick as annotate_next_access but keyed on the
  // session rather than the block: walking backwards, the most recent index at
  // which a session was seen is that session's *next* use.
  std::unordered_map<uint64_t, int64_t> next_seen;
  for (int64_t i = static_cast<int64_t>(requests.size()) - 1; i >= 0; --i) {
    Request &r = requests[static_cast<size_t>(i)];
    auto it = next_seen.find(r.session);
    r.session_next_access = it == next_seen.end() ? INT64_MAX : it->second;
    next_seen[r.session] = i;
  }
}

}  // namespace prefixsim

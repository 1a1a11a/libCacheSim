// Trace ingestion for prefixsim.
//
// A prefix-cache trace is a sequence of LLM requests, and each request is an
// ordered list of KV-block identities: index 0 is the prefix root (the first
// block of the prompt), the last index is the deepest, most private block.
//
// This deliberately does NOT use libCacheSim's reader_t. A reader_t emits one
// request_t (one object) at a time and has no way to express "these N objects
// form one request that must be resident together", which is the entire point
// of this simulator. Adding an input format means writing a TraceReader and
// adding one line to parse_trace_format()/open_trace().

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "libCacheSim/request.h"
}

namespace prefixsim {

/// One LLM request (one prompt / one turn) expanded into its KV blocks.
struct Request {
  int64_t index = 0;             ///< 0-based ordinal of this request in the trace.
  double timestamp = 0.0;        ///< Arrival time in seconds as recorded in the trace.
  std::vector<obj_id_t> blocks;  ///< Block identities in prefix order (root first).

  /// Opaque id of the request's workload class, for policies that fit a
  /// separate reuse-time distribution per class. Folded from the trace's
  /// `type` and `turn` fields -- the (request type, turn number) pair that
  /// arXiv:2506.02634 calls a request category. 0 when the trace carries
  /// neither field, which every such policy must treat as "one class".
  uint64_t category = 0;

  /// Logical next-access virtual time of each block, filled by
  /// annotate_next_access(). Indexed in lockstep with `blocks`.
  /// See README.md "Next-access annotation" for why this uses forward order
  /// even though blocks are replayed in reverse.
  std::vector<int64_t> next_access_vtime;
};

/// How a block in the trace becomes a cache object identity.
enum class BlockIdMode {
  /// Fold every block id of the prefix into a rolling hash, so a block's
  /// identity is "the whole path from the root up to and including me". This is
  /// what a real prefix cache does, and it is the default.
  kPrefixHash,
  /// Use the trace's raw hash id verbatim. Only correct when the trace's ids
  /// are already prefix-unique; the qwen traces are not. See README.md.
  kRaw,
};

/// Abstract trace source. next() returns false at end of trace.
class TraceReader {
 public:
  virtual ~TraceReader() = default;
  virtual bool next(Request &out) = 0;
  virtual const char *format_name() const = 0;
};

enum class TraceFormat {
  kQwenJsonl,  ///< JSONL, one object per request, carrying a `hash_ids` array.
};

bool parse_trace_format(const std::string &name, TraceFormat &out);
const char *trace_format_name(TraceFormat format);

/// Open `path` as `format`. Returns nullptr and sets `error` on failure.
std::unique_ptr<TraceReader> open_trace(const std::string &path, TraceFormat format,
                                        BlockIdMode mode, std::string &error);

/// Read a whole trace into memory and annotate next-access times.
bool load_trace(TraceReader &reader, std::vector<Request> &out, std::string &error);

/// Fill next_access_vtime for every block of every request.
///
/// Virtual time is the 1-based index of a block access in the *forward*
/// flattened trace: request 0 block 0 is vtime 1, request 0 block 1 is vtime 2,
/// and so on. A block whose id never recurs gets MAX_REUSE_DISTANCE, matching
/// what libCacheSim's own trace readers use for "no next access".
void annotate_next_access(std::vector<Request> &requests);

}  // namespace prefixsim

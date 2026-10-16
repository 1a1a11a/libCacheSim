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
//
// Two ways to consume a trace. A JSONL trace carries no next-access times, so
// load_trace() holds it in memory and computes them in a reverse pass. An
// lcp trace stores them, so its reader is annotated() and the simulator
// streams it: one forward pass, one request in memory at a time.

#pragma once

#include <climits>
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
  int64_t index = 0;  ///< 0-based ordinal of this request in the trace.
  /// The trace's `timestamp`, in seconds: completion time in fi-new and
  /// chutes (when the request's blocks are registered in the cache), arrival
  /// time in the Qwen traces. Requests are ordered by it.
  double timestamp = 0.0;
  std::vector<obj_id_t>
      blocks;  ///< Block identities in prefix order (root first).

  /// Conversation this request belongs to: the trace's `session_id` string,
  /// interned to 1, 2, 3, ... in order of first appearance. 0 when the trace
  /// has no session field. Session-level eviction policies group blocks by
  /// this.
  uint64_t session = 0;

  /// Index of the next request from the SAME session, or INT64_MAX if this is
  /// the session's last. The session-level analogue of next_access_vtime, and
  /// the only thing a session-granular Belady oracle needs.
  int64_t session_next_access = INT64_MAX;

  /// The request's workload class, for policies that fit a separate reuse-time
  /// distribution per class: the (request type, turn number) pair that
  /// arXiv:2506.02634 calls a request category, interned to 1, 2, 3, ... in
  /// order of first appearance. 0 when the trace carries neither field, which
  /// every such policy must treat as "one class".
  uint64_t category = 0;

  /// Descriptive fields carried through to the lcp format. No policy reads
  /// them directly (`type` and `turn` reach policies through `category`); -1
  /// (or an empty `type`) means the trace lacks the field or holds null there.
  int64_t chat_id = -1;
  int64_t parent_chat_id = -1;
  int32_t input_length = -1;
  int32_t output_length = -1;
  int32_t turn = -1;
  std::string type;

  /// Logical next-access virtual time of each block, filled by
  /// annotate_next_access(). Indexed in lockstep with `blocks`.
  /// See README.md "Next-access annotation" for why this uses forward order
  /// even though blocks are replayed in reverse.
  std::vector<int64_t> next_access_vtime;
};

/// How a block in the trace becomes a cache object identity.
enum class BlockIdMode {
  /// Positional encoding: a block's identity is "the whole path from the root
  /// up to and including me", which is what a real prefix cache keys on, and
  /// the default. Exact, not hashed: each (parent id, raw id) pair is interned
  /// to the next integer, so ids are 1, 2, 3, ... in order of first appearance
  /// and two blocks share one only when their whole paths match. Identical to
  /// the ids an lcp trace stores.
  kPrefix,
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

  /// True when next() fills next_access_vtime and session_next_access itself,
  /// so the trace can be simulated as it streams, without load_trace().
  virtual bool annotated() const { return false; }
  /// Requests and block accesses in the whole trace, when the format records
  /// them up front; -1 otherwise.
  virtual int64_t n_requests_total() const { return -1; }
  virtual int64_t n_blocks_total() const { return -1; }
};

enum class TraceFormat {
  kQwenJsonl,  ///< JSONL, one object per request, carrying a `hash_ids` array.
  kLcp,        ///< lcp binary, plain or .zst; see doc/lcp_format.md.
};

bool parse_trace_format(const std::string &name, TraceFormat &out);
const char *trace_format_name(TraceFormat format);
/// The format a file name implies: lcp if it contains ".lcp", else JSONL.
TraceFormat guess_trace_format(const std::string &path);

/// Open `path` as `format`. Returns nullptr and sets `error` on failure.
std::unique_ptr<TraceReader> open_trace(const std::string &path,
                                        TraceFormat format, BlockIdMode mode,
                                        std::string &error);

/// Read a whole trace into memory and annotate next-access times, unless the
/// reader already did.
bool load_trace(TraceReader &reader, std::vector<Request> &out,
                std::string &error);

/// Fill next_access_vtime for every block of every request.
///
/// Virtual time is the 1-based index of a block access in the *forward*
/// flattened trace: request 0 block 0 is vtime 1, request 0 block 1 is vtime 2,
/// and so on. A block whose id never recurs gets MAX_REUSE_DISTANCE, matching
/// what libCacheSim's own trace readers use for "no next access".
void annotate_next_access(std::vector<Request> &requests);

/// Fill session_next_access for every request, in one reverse pass.
void annotate_session_next_access(std::vector<Request> &requests);

}  // namespace prefixsim

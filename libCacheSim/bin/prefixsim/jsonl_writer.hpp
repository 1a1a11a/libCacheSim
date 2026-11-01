// Writing a trace back out as qwen-style JSONL, the inverse of traceConvLLM's
// JSONL -> lcp: a released lcp file can then be read by JSONL tools, filtered
// (a time window, a set of sessions, ...), and converted back.
//
// Each line carries the fields the JSONL reader takes, so converting the
// output back gives the same requests: chat_id, parent_chat_id, timestamp
// (completion) and timestamp_arrival in seconds to the microsecond,
// input_length, output_length, type and turn (null or absent when the trace
// lacks them), session_id and user_id as strings of their interned ids
// (absent for 0), and hash_ids. `hash_ids` holds the trace's block ids: for
// lcp the positional ids, not the source's hashes, which lcp does not keep.
// Read back with --block-id prefix, they intern to themselves.

#pragma once

#include <cstdint>
#include <string>

#include "trace.hpp"

namespace prefixsim {

/// Append `request` as one JSON object, without a newline, to `out`.
void append_jsonl(const Request &request, std::string &out);

/// Stream every request `reader` yields to `path`, one per line. Sets
/// `n_written`; returns false with `error` set if the file cannot be written.
bool write_jsonl(TraceReader &reader, const std::string &path,
                 int64_t &n_written, std::string &error);

}  // namespace prefixsim

// Writing the lcp trace format (doc/lcp_format.md).
//
// Lives beside the readers so the converter, prefixsim and the tests share one
// definition of what a trace means: the requests written are exactly the ones
// load_trace() produced, ids, sessions and next access included.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "trace.hpp"

extern "C" {
#include "traceReader/customizedReader/lcp.h"
}

namespace prefixsim {

/// Write `requests`, as load_trace() returns them with prefix ids, to `path`.
/// Fills `stat` with the header's stat block. Returns false with `error` set
/// when the trace cannot be represented (time going backwards, too many
/// request types, ...) or the write fails.
bool write_lcp(const std::vector<Request> &requests, int32_t block_size_tokens,
               const std::string &path, lcp_trace_stat_t &stat,
               std::string &error);

}  // namespace prefixsim

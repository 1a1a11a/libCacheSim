#pragma once

#include "libCacheSim/reader.h"

/* Oracle timestamps describe the original stream. Dropping a request can
 * strand a cached object's next-access pointer and changes reuse distances.
 * Recompute offline rather than guessing that a zero-size row is a delete. */
static inline void oracle_warn_zero_size(reader_t *reader) {
  if (reader->init_params.require_valid_next_access) {
    LOGGING(
        ERROR_LEVEL,
        "oracle trace: filtering zero-size records invalidates "
        "next_access_vtime; Belady/BeladySize results are unreliable. "
        "Decompress if needed, then repair with traceConv INPUT "
        "TRACE_TYPE --output-format=oracleGeneral -o REPAIRED.oracleGeneral. "
        "See doc/oracle_traces.md and issue #361.\n");
    exit(EXIT_FAILURE);
  }
  if (!reader->warned_oracle_zero_size) {
    WARN(
        "oracle trace: skipping zero-size records without recomputing "
        "next_access_vtime; Belady/BeladySize results are unreliable. "
        "Repair with traceConv; see doc/oracle_traces.md and issue #361.\n");
    reader->warned_oracle_zero_size = true;
  }
}

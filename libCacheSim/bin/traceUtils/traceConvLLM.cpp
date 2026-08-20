// traceConvLLM -- convert a qwen-style JSONL LLM trace to the lcsllm binary
// format. See doc/lcsllm_format.md.
//
// The JSONL parsing, positional encoding and next-access annotation are reused
// from prefixsim's trace reader rather than reimplemented, so the converter and
// the simulator can never disagree about what a trace means.
//
// Block ids are always POSITIONALLY ENCODED: an id is a hash folded over the
// whole path from the root up to and including that block, so two blocks share
// an id only when the block *and its entire prefix* are identical. That is not
// an option here, it is what the format guarantees -- see the enforcement note
// below and doc/lcsllm_format.md.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../prefixsim/trace.hpp"

extern "C" {
#include "traceReader/customizedReader/lcsllmReader.h"
}

namespace {

struct Options {
  std::string input;
  std::string output;
  prefixsim::TraceFormat format = prefixsim::TraceFormat::kQwenJsonl;
  int32_t block_size_tokens = 16;
  std::string type_name = "unknown";
};

/// Report one positional-encoding violation, capped so a systematically broken
/// trace does not print millions of lines. Increments the count either way.
void report_violation(int64_t &n_violations, obj_id_t id, int64_t request,
                      size_t position, const char *what) {
  constexpr int64_t kMaxReported = 5;
  if (n_violations < kMaxReported) {
    fprintf(stderr,
            "error: block id %llu %s -- request %" PRId64 ", position %zu\n",
            (unsigned long long)id, what, request, position);
  } else if (n_violations == kMaxReported) {
    fprintf(stderr, "error: ... further violations not listed\n");
  }
  ++n_violations;
}

void print_usage(const char *program) {
  printf("Usage: %s --input <trace> --output <trace.lcsllm> [options]\n\n", program);
  printf("Convert an LLM request trace to the lcsllm binary format.\n\n");
  printf("Required:\n");
  printf("  --input <path>            Source trace.\n");
  printf("  --output <path>           Destination .lcsllm file.\n\n");
  printf("Options:\n");
  printf("  --input-format <fmt>      Source format (default: qwen-jsonl).\n");
  printf("  --block-size-tokens <n>   Tokens per KV block, recorded in the\n");
  printf("                            header (default: 16).\n");
  printf("  --type <name>             Request-type name for the header table\n");
  printf("                            (default: unknown).\n");
  printf("  --help                    Print this message.\n");
}

bool parse_args(int argc, char **argv, Options &opts, bool &should_exit) {
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      print_usage(argv[0]);
      should_exit = true;
      return true;
    }
    if (i + 1 >= argc) {
      fprintf(stderr, "error: %s requires a value\n", arg);
      return false;
    }
    const char *value = argv[++i];

    if (strcmp(arg, "--input") == 0) {
      opts.input = value;
    } else if (strcmp(arg, "--output") == 0) {
      opts.output = value;
    } else if (strcmp(arg, "--type") == 0) {
      opts.type_name = value;
    } else if (strcmp(arg, "--input-format") == 0) {
      if (!prefixsim::parse_trace_format(value, opts.format)) {
        fprintf(stderr, "error: unknown --input-format '%s'\n", value);
        return false;
      }
    } else if (strcmp(arg, "--block-id") == 0) {
      // Was an option; now a guarantee. Fail loudly rather than accept a flag
      // that no longer does anything -- a script passing --block-id raw was
      // asking for output this format can no longer represent, and silently
      // giving it positionally encoded ids instead would be worse than an error.
      fprintf(stderr,
              "error: --block-id was removed. lcsllm block ids are always "
              "positionally encoded (an id is a hash over the whole path from "
              "the root), because a prefix cache keys on a block *and its "
              "prefix*; raw ids cannot express that. Drop the flag.\n");
      return false;
    } else if (strcmp(arg, "--block-size-tokens") == 0) {
      opts.block_size_tokens = static_cast<int32_t>(strtol(value, nullptr, 10));
    } else {
      fprintf(stderr, "error: unknown argument '%s'\n", arg);
      return false;
    }
  }

  if (opts.input.empty() || opts.output.empty()) {
    fprintf(stderr, "error: --input and --output are required\n");
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  Options opts;
  bool should_exit = false;
  if (!parse_args(argc, argv, opts, should_exit)) return 1;
  if (should_exit) return 0;

  std::string error;
  auto reader = prefixsim::open_trace(opts.input, opts.format,
                                     prefixsim::BlockIdMode::kPrefixHash, error);
  if (reader == nullptr) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  // load_trace also fills next_access_vtime, in one reverse pass over the
  // forward-flattened block sequence -- exactly the vtime the format specifies.
  std::vector<prefixsim::Request> requests;
  if (!prefixsim::load_trace(*reader, requests, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  // ---- enforce the positional encoding, rather than assume it ----
  //
  // The encoding is deterministic, so the only way it can be violated is a
  // 64-bit hash collision between two distinct paths. That failure is silent and
  // corrupting -- two unrelated prefixes would share one cache object -- so it is
  // checked here, where the predecessor of every block is still known. A
  // consumer of the file cannot perform this check at all.
  //
  // Checking "each id has one predecessor" is enough to prove full path
  // uniqueness, by induction: if two paths reach the same id and agree on the
  // previous id, then they agree one step earlier too, and that id passed the
  // same test. Position-0 blocks have no predecessor and are tracked separately,
  // so a folded id deep in one request cannot quietly equal a root id of
  // another.
  int64_t n_blocks = 0, max_blocks = 0;
  std::unordered_map<obj_id_t, obj_id_t> predecessor;
  std::unordered_set<obj_id_t> roots;
  int64_t n_violations = 0;

  for (const prefixsim::Request &r : requests) {
    const int64_t n = static_cast<int64_t>(r.blocks.size());
    n_blocks += n;
    if (n > max_blocks) max_blocks = n;

    for (size_t i = 0; i < r.blocks.size(); ++i) {
      const obj_id_t id = r.blocks[i];

      if (i == 0) {
        // A root has no predecessor, so any id already recorded with one is a
        // collision between an empty prefix and a non-empty one.
        if (predecessor.count(id) != 0) {
          report_violation(n_violations, id, r.index, i,
                           "appears both as a prefix root and deeper in a path");
        }
        roots.insert(id);
        continue;
      }

      const obj_id_t pred = r.blocks[i - 1];
      const auto it = predecessor.find(id);
      if (it == predecessor.end()) {
        if (roots.count(id) != 0) {
          report_violation(n_violations, id, r.index, i,
                           "appears both deeper in a path and as a prefix root");
        }
        predecessor.emplace(id, pred);
      } else if (it->second != pred) {
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "is reached from two different predecessors, %llu and %llu",
                 (unsigned long long)it->second, (unsigned long long)pred);
        report_violation(n_violations, id, r.index, i, detail);
      }
    }
  }

  if (n_violations > 0) {
    fprintf(stderr,
            "error: %" PRId64
            " block ids are not positionally encoded, so this trace cannot be "
            "written as lcsllm. With a deterministic path hash this means a "
            "64-bit collision; report it rather than working around it.\n",
            n_violations);
    return 1;
  }

  const int64_t n_unique = static_cast<int64_t>(predecessor.size() + roots.size());

  FILE *out = fopen(opts.output.c_str(), "wb");
  if (out == nullptr) {
    fprintf(stderr, "error: cannot open '%s' for writing\n", opts.output.c_str());
    return 1;
  }

  lcsllm_header_t header;
  memset(&header, 0, sizeof(header));
  header.start_magic = LCSLLM_MAGIC;
  header.end_magic = LCSLLM_MAGIC;
  header.version = LCSLLM_CURR_VERSION;
  header.n_requests = static_cast<int64_t>(requests.size());
  header.n_blocks = n_blocks;
  header.n_unique_blocks = n_unique;
  header.max_blocks_per_request = max_blocks;
  header.start_time_us = static_cast<int64_t>(requests.front().timestamp * 1e6);
  header.end_time_us = static_cast<int64_t>(requests.back().timestamp * 1e6);
  header.block_size_tokens = opts.block_size_tokens;
  header.block_id_kind = LCSLLM_BLOCK_ID_PREFIX;
  header.has_next_access = 1;
  header.n_types = 1;
  snprintf(header.type_names[0], LCSLLM_TYPE_LEN, "%s", opts.type_name.c_str());

  if (fwrite(&header, sizeof(header), 1, out) != 1) {
    fprintf(stderr, "error: writing header failed\n");
    fclose(out);
    return 1;
  }

  std::vector<lcsllm_block_t> blocks;
  blocks.reserve(static_cast<size_t>(max_blocks));
  for (const prefixsim::Request &r : requests) {
    lcsllm_req_header_t rh;
    memset(&rh, 0, sizeof(rh));
    // The qwen format has no explicit chat id per request in every variant, so
    // fall back to the request index; parent/turn default to "unknown".
    rh.chat_id = static_cast<uint64_t>(r.index);
    rh.parent_chat_id = -1;
    rh.timestamp_us = static_cast<int64_t>(r.timestamp * 1e6);
    rh.n_blocks = static_cast<uint32_t>(r.blocks.size());
    rh.turn = 0;
    rh.type_id = 0;

    blocks.clear();
    for (size_t i = 0; i < r.blocks.size(); ++i) {
      lcsllm_block_t b;
      b.block_id = r.blocks[i];
      // load_trace uses MAX_REUSE_DISTANCE for "never"; the format uses -1.
      b.next_access_vtime = r.next_access_vtime[i] >= MAX_REUSE_DISTANCE
                                ? -1
                                : r.next_access_vtime[i];
      blocks.push_back(b);
    }

    if (fwrite(&rh, sizeof(rh), 1, out) != 1 ||
        (!blocks.empty() &&
         fwrite(blocks.data(), sizeof(lcsllm_block_t), blocks.size(), out) !=
             blocks.size())) {
      fprintf(stderr, "error: writing request %" PRId64 " failed\n", r.index);
      fclose(out);
      return 1;
    }
  }

  fclose(out);
  printf("wrote %s: %" PRId64 " requests, %" PRId64 " blocks, %" PRId64
         " unique, max %" PRId64
         " blocks/request, block ids positionally encoded (verified)\n",
         opts.output.c_str(), header.n_requests, header.n_blocks,
         header.n_unique_blocks, header.max_blocks_per_request);
  return 0;
}

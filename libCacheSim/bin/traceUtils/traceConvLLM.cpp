// traceConvLLM -- convert a qwen-style JSONL LLM trace to the lcsllm binary
// format. See doc/lcsllm_format.md.
//
// The JSONL parsing, prefix-hash folding and next-access annotation are reused
// from prefixsim's trace reader rather than reimplemented, so the converter and
// the simulator can never disagree about what a trace means.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
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
  prefixsim::BlockIdMode block_id = prefixsim::BlockIdMode::kPrefixHash;
  int32_t block_size_tokens = 16;
  std::string type_name = "unknown";
};

void print_usage(const char *program) {
  printf("Usage: %s --input <trace> --output <trace.lcsllm> [options]\n\n", program);
  printf("Convert an LLM request trace to the lcsllm binary format.\n\n");
  printf("Required:\n");
  printf("  --input <path>            Source trace.\n");
  printf("  --output <path>           Destination .lcsllm file.\n\n");
  printf("Options:\n");
  printf("  --input-format <fmt>      Source format (default: qwen-jsonl).\n");
  printf("  --block-id <mode>         prefix-hash (default) | raw.\n");
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
      if (strcmp(value, "prefix-hash") == 0) {
        opts.block_id = prefixsim::BlockIdMode::kPrefixHash;
      } else if (strcmp(value, "raw") == 0) {
        opts.block_id = prefixsim::BlockIdMode::kRaw;
      } else {
        fprintf(stderr, "error: unknown --block-id '%s'\n", value);
        return false;
      }
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
  auto reader = prefixsim::open_trace(opts.input, opts.format, opts.block_id, error);
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

  int64_t n_blocks = 0, max_blocks = 0;
  std::unordered_set<obj_id_t> unique;
  for (const prefixsim::Request &r : requests) {
    const int64_t n = static_cast<int64_t>(r.blocks.size());
    n_blocks += n;
    if (n > max_blocks) max_blocks = n;
    unique.insert(r.blocks.begin(), r.blocks.end());
  }

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
  header.n_unique_blocks = static_cast<int64_t>(unique.size());
  header.max_blocks_per_request = max_blocks;
  header.start_time_us = static_cast<int64_t>(requests.front().timestamp * 1e6);
  header.end_time_us = static_cast<int64_t>(requests.back().timestamp * 1e6);
  header.block_size_tokens = opts.block_size_tokens;
  header.block_id_kind = opts.block_id == prefixsim::BlockIdMode::kPrefixHash
                             ? LCSLLM_BLOCK_ID_PREFIX
                             : LCSLLM_BLOCK_ID_RAW;
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
         " unique, max %" PRId64 " blocks/request, block-id %s\n",
         opts.output.c_str(), header.n_requests, header.n_blocks,
         header.n_unique_blocks, header.max_blocks_per_request,
         header.block_id_kind == LCSLLM_BLOCK_ID_PREFIX ? "prefix-hash" : "raw");
  return 0;
}

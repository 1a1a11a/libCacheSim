// traceConvLLM -- convert a qwen-style JSONL LLM trace to the lcp binary
// format. See doc/lcp_format.md.
//
// Parsing, positional encoding, next-access annotation and writing are
// prefixsim's own (bin/prefixsim), so the converter and the simulator cannot
// disagree about what a trace means: the block, session and category ids
// written here are exactly the ones prefixsim assigns when it reads the same
// JSONL, and prefixsim reading the output gives the same requests back.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../prefixsim/lcp_writer.hpp"
#include "../prefixsim/trace.hpp"

namespace {

struct Options {
  std::string input;
  std::string output;
  prefixsim::TraceFormat format = prefixsim::TraceFormat::kQwenJsonl;
  int32_t block_size_tokens = 16;
};

void print_usage(const char *program) {
  printf("Usage: %s --input <trace> --output <trace.lcp> [options]\n\n",
         program);
  printf("Convert an LLM request trace to the lcp binary format.\n\n");
  printf("Required:\n");
  printf("  --input <path>            Source trace.\n");
  printf("  --output <path>           Destination .lcp file.\n\n");
  printf("Options:\n");
  printf("  --input-format <fmt>      Source format (default: qwen-jsonl).\n");
  printf("  --block-size-tokens <n>   Tokens per KV block, recorded in the\n");
  printf("                            header (default: 16).\n");
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
    } else if (strcmp(arg, "--input-format") == 0) {
      if (!prefixsim::parse_trace_format(value, opts.format)) {
        fprintf(stderr, "error: unknown --input-format '%s'\n", value);
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
  if (opts.block_size_tokens <= 0) {
    fprintf(stderr, "error: --block-size-tokens must be positive\n");
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
                                      prefixsim::BlockIdMode::kPrefix, error);
  if (reader == nullptr) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  // load_trace also fills next_access_vtime and session_next_access, in
  // reverse passes over the whole trace: the future the format stores so that
  // its readers need only one forward pass.
  std::vector<prefixsim::Request> requests;
  if (!prefixsim::load_trace(*reader, requests, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  lcp_trace_stat_t st;
  if (!prefixsim::write_lcp(requests, opts.block_size_tokens, opts.output, st,
                            error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  printf("wrote %s: %" PRId64 " requests, %" PRId64 " blocks, %" PRId64
         " unique, %" PRId64 " sessions, %" PRId64 " types, max %" PRId64
         " blocks/request\n",
         opts.output.c_str(), st.n_req, st.n_block, st.n_unique_block,
         st.n_session, st.n_type, st.max_blocks_per_req);
  return 0;
}

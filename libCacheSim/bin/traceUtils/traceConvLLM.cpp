// traceConvLLM -- convert a qwen-style JSONL LLM trace to the lcp binary
// format, or an lcp trace back to JSONL. See doc/lcp_format.md.
//
// Parsing, positional encoding, next-access annotation and writing are
// prefixsim's own (bin/prefixsim), so the converter and the simulator cannot
// disagree about what a trace means: the block, session and category ids
// written here are exactly the ones prefixsim assigns when it reads the same
// JSONL, and prefixsim reading the output gives the same requests back.
// Converting lcp -> JSONL -> lcp reproduces the file.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../prefixsim/jsonl_writer.hpp"
#include "../prefixsim/lcp_writer.hpp"
#include "../prefixsim/trace.hpp"

namespace {

struct Options {
  std::string input;
  std::string output;
  prefixsim::TraceFormat format = prefixsim::TraceFormat::kQwenJsonl;
  bool format_given = false;
  int32_t block_size_tokens = 16;
  bool compress = true;
};

bool ends_with(const std::string &s, const char *suffix) {
  const size_t n = strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

void print_usage(const char *program) {
  printf("Usage: %s --input <trace> --output <path> [options]\n\n", program);
  printf("Convert a JSONL LLM request trace to the lcp binary format, or an\n");
  printf("lcp trace back to JSONL. The input's format sets the direction.\n\n");
  printf("Required:\n");
  printf("  --input <path>            Source trace.\n");
  printf("  --output <path>           Destination. lcp is written\n");
  printf("                            zstd-compressed (as zstd -8 --long);\n");
  printf("                            .zst is appended if missing, since\n");
  printf("                            readers detect zstd by it. JSONL is\n");
  printf("                            written plain.\n\n");
  printf("Options:\n");
  printf("  --input-format <fmt>      qwen-jsonl | lcp (default: lcp if the\n");
  printf("                            file name contains .lcp, else\n");
  printf("                            qwen-jsonl).\n");
  printf("  --block-size-tokens <n>   Tokens per KV block, recorded in the\n");
  printf("                            lcp header (default: 16).\n");
  printf("  --no-zstd                 Write a plain, uncompressed lcp file.\n");
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
    if (strcmp(arg, "--no-zstd") == 0) {
      opts.compress = false;
      continue;
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
      opts.format_given = true;
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
  if (!opts.format_given) {
    opts.format = prefixsim::guess_trace_format(opts.input);
  }
  if (opts.format == prefixsim::TraceFormat::kLcp) {
    if (ends_with(opts.output, ".zst")) {
      fprintf(stderr,
              "error: JSONL is written plain, since the JSONL readers take "
              "only plain files; drop the .zst\n");
      return false;
    }
  } else if (opts.compress && !ends_with(opts.output, ".zst")) {
    opts.output += ".zst";
  } else if (!opts.compress && ends_with(opts.output, ".zst")) {
    fprintf(stderr,
            "error: --no-zstd with an output ending in .zst; readers would "
            "take the plain file for zstd\n");
    return false;
  }
  return true;
}

/// lcp -> JSONL, streamed one request at a time.
int to_jsonl(prefixsim::TraceReader &reader, const Options &opts) {
  std::string error;
  int64_t n = 0;
  if (!prefixsim::write_jsonl(reader, opts.output, n, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }
  printf("wrote %s: %" PRId64 " requests\n", opts.output.c_str(), n);
  if (reader.block_size_tokens() != 16) {
    printf(
        "the blocks are %d tokens: pass --block-size-tokens %d when "
        "converting back\n",
        reader.block_size_tokens(), reader.block_size_tokens());
  }
  return 0;
}

/// JSONL -> lcp. The whole trace is held, to compute next access.
int to_lcp(prefixsim::TraceReader &reader, const Options &opts) {
  // load_trace also fills next_access_vtime and session_next_access, in
  // reverse passes over the whole trace: the future the format stores so that
  // its readers need only one forward pass.
  std::string error;
  std::vector<prefixsim::Request> requests;
  if (!prefixsim::load_trace(reader, requests, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  lcp_trace_stat_t st;
  if (!prefixsim::write_lcp(requests, opts.block_size_tokens, opts.output,
                            opts.compress, st, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  printf("wrote %s: %" PRId64 " requests, %" PRId64 " blocks, %" PRId64
         " unique, %" PRId64 " sessions, %" PRId64 " users, %" PRId64
         " types, max %" PRId64 " blocks/request\n",
         opts.output.c_str(), st.n_req, st.n_block, st.n_unique_block,
         st.n_session, st.n_user, st.n_type, st.max_blocks_per_req);
  return 0;
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
  return opts.format == prefixsim::TraceFormat::kLcp ? to_jsonl(*reader, opts)
                                                     : to_lcp(*reader, opts);
}

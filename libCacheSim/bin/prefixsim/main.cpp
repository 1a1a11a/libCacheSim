// prefixsim -- a prefix-cache simulator for LLM serving traces.
//
// Reports block hit ratio and compute saving ratio for one or more eviction
// algorithms over an LLM request trace, modelling the constraint that a request
// needs all of its KV blocks resident at the same time.
//
// See README.md in this directory for the design.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cost_model.hpp"
#include "simulator.hpp"
#include "trace.hpp"

extern "C" {
#include "libCacheSim/logging.h"
}

namespace {

struct Options {
  std::string trace_path;
  std::string output_path;
  std::vector<std::string> algorithms{"lru"};
  int64_t cache_size = 0;
  prefixsim::TraceFormat format = prefixsim::TraceFormat::kQwenJsonl;
  prefixsim::CostModel cost_model = prefixsim::CostModel::kUniform;
  prefixsim::BlockIdMode block_id = prefixsim::BlockIdMode::kPrefixHash;
  bool verify = true;
};

void print_usage(const char *program) {
  printf("Usage: %s --trace <path> --cache-size <n_blocks> [options]\n\n", program);
  printf("Simulate an LLM prefix cache: report block hit ratio and compute\n");
  printf("saving ratio, honouring the constraint that a request needs all of\n");
  printf("its blocks resident at once.\n\n");
  printf("Required:\n");
  printf("  --trace <path>          Trace file.\n");
  printf("  --cache-size <n>        Cache capacity in BLOCKS (suffixes k/m/g = x1024).\n\n");
  printf("Options:\n");
  printf("  --algo <a,b,c>          Eviction algorithms, comma separated (default: lru).\n");
  printf("  --trace-format <fmt>    Input format (default: qwen-jsonl).\n");
  printf("  --cost-model <m>        uniform | position | affine (default: uniform).\n");
  printf("  --block-id <mode>       prefix-hash | raw (default: prefix-hash).\n");
  printf("  --no-verify             Skip the post-request residency check.\n");
  printf("  --output <path>         Also append the RESULT lines to this file.\n");
  printf("  --list-algos            Print supported algorithms and exit.\n");
  printf("  --help                  Print this message.\n\n");
  printf("Example:\n");
  printf("  %s --trace qwen_traceA_blksz_16.jsonl --cache-size 8k \\\n", program);
  printf("      --algo lru,random_compute,belady_compute --cost-model position\n");
}

void print_algos() {
  printf("supported eviction algorithms:\n");
  for (const std::string &name : prefixsim::supported_algorithms()) {
    printf("  %s\n", name.c_str());
  }
  printf(
      "\nOnly algorithms that keep their whole resident set in the main hash\n"
      "table are listed: the phase-1 match probes that table directly. See\n"
      "README.md \"Which algorithms work\".\n");
}

/// Parse "8", "8k", "2m". Suffixes are powers of 1024, matching cachesim.
bool parse_size(const char *text, int64_t &out) {
  char *end = nullptr;
  errno = 0;
  long long value = strtoll(text, &end, 10);
  if (end == text || value <= 0 || errno != 0) return false;

  switch (*end) {
    case '\0':
      break;
    case 'k':
    case 'K':
      value *= 1024;
      ++end;
      break;
    case 'm':
    case 'M':
      value *= 1024LL * 1024;
      ++end;
      break;
    case 'g':
    case 'G':
      value *= 1024LL * 1024 * 1024;
      ++end;
      break;
    default:
      return false;
  }
  if (*end != '\0') return false;
  out = static_cast<int64_t>(value);
  return true;
}

std::vector<std::string> split_commas(const std::string &text) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t comma = text.find(',', start);
    const size_t end = (comma == std::string::npos) ? text.size() : comma;
    if (end > start) parts.emplace_back(text.substr(start, end - start));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return parts;
}

bool parse_args(int argc, char **argv, Options &opts, bool &should_exit) {
  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    const bool has_value = (i + 1 < argc);

    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      print_usage(argv[0]);
      should_exit = true;
      return true;
    }
    if (strcmp(arg, "--list-algos") == 0) {
      print_algos();
      should_exit = true;
      return true;
    }
    if (strcmp(arg, "--no-verify") == 0) {
      opts.verify = false;
      continue;
    }

    if (!has_value) {
      fprintf(stderr, "error: %s requires a value\n", arg);
      return false;
    }
    const char *value = argv[++i];

    if (strcmp(arg, "--trace") == 0 || strcmp(arg, "--trace-file") == 0) {
      opts.trace_path = value;
    } else if (strcmp(arg, "--output") == 0) {
      opts.output_path = value;
    } else if (strcmp(arg, "--algo") == 0) {
      opts.algorithms = split_commas(value);
    } else if (strcmp(arg, "--cache-size") == 0) {
      if (!parse_size(value, opts.cache_size)) {
        fprintf(stderr, "error: bad --cache-size '%s'\n", value);
        return false;
      }
    } else if (strcmp(arg, "--trace-format") == 0) {
      if (!prefixsim::parse_trace_format(value, opts.format)) {
        fprintf(stderr, "error: unknown --trace-format '%s'\n", value);
        return false;
      }
    } else if (strcmp(arg, "--cost-model") == 0) {
      if (!prefixsim::parse_cost_model(value, opts.cost_model)) {
        fprintf(stderr, "error: unknown --cost-model '%s'\n", value);
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
    } else {
      fprintf(stderr, "error: unknown argument '%s'\n", arg);
      return false;
    }
  }

  if (opts.trace_path.empty()) {
    fprintf(stderr, "error: --trace is required\n");
    return false;
  }
  if (opts.cache_size <= 0) {
    fprintf(stderr, "error: --cache-size is required\n");
    return false;
  }
  return true;
}

void report(FILE *out, const std::string &algorithm, const Options &opts,
            const prefixsim::Stats &stats) {
  fprintf(out,
          "RESULT trace=%s algo=%s cache_size=%lld cost_model=%s block_id=%s "
          "n_req=%lld n_req_skipped=%lld n_blocks=%lld "
          "block_hit_ratio=%.6f compute_saving_ratio=%.6f "
          "n_eviction=%lld n_self_eviction=%lld n_unexpected_eviction=%lld\n",
          opts.trace_path.c_str(), algorithm.c_str(),
          static_cast<long long>(opts.cache_size),
          prefixsim::cost_model_name(opts.cost_model),
          opts.block_id == prefixsim::BlockIdMode::kPrefixHash ? "prefix-hash" : "raw",
          static_cast<long long>(stats.n_requests),
          static_cast<long long>(stats.n_requests_skipped),
          static_cast<long long>(stats.n_blocks), stats.block_hit_ratio(),
          stats.compute_saving_ratio(),
          static_cast<long long>(stats.n_evictions),
          static_cast<long long>(stats.n_self_evictions),
          static_cast<long long>(stats.n_unexpected_evictions));
}

}  // namespace

int main(int argc, char **argv) {
  Options opts;
  bool should_exit = false;
  if (!parse_args(argc, argv, opts, should_exit)) return 1;
  if (should_exit) return 0;

  if (opts.block_id == prefixsim::BlockIdMode::kRaw &&
      prefixsim::cost_is_position_dependent(opts.cost_model)) {
    WARN(
        "--block-id raw with a position-dependent cost model: a raw block id "
        "reached through two different prefixes gets two different costs, and "
        "cache_find_base() aborts on a cost change. Use prefix-hash, or "
        "--cost-model uniform.\n");
  }

  std::string error;
  auto reader = prefixsim::open_trace(opts.trace_path, opts.format, opts.block_id, error);
  if (reader == nullptr) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  std::vector<prefixsim::Request> requests;
  if (!prefixsim::load_trace(*reader, requests, error)) {
    fprintf(stderr, "error: %s\n", error.c_str());
    return 1;
  }

  int64_t n_blocks_total = 0;
  for (const prefixsim::Request &r : requests) {
    n_blocks_total += static_cast<int64_t>(r.blocks.size());
  }
  printf("trace %s: %lld requests, %lld block accesses, format %s, block-id %s\n",
         opts.trace_path.c_str(), static_cast<long long>(requests.size()),
         static_cast<long long>(n_blocks_total),
         prefixsim::trace_format_name(opts.format),
         opts.block_id == prefixsim::BlockIdMode::kPrefixHash ? "prefix-hash" : "raw");
  printf("cache size %lld blocks, cost model %s\n\n",
         static_cast<long long>(opts.cache_size),
         prefixsim::cost_model_name(opts.cost_model));

  FILE *out_file = nullptr;
  if (!opts.output_path.empty()) {
    out_file = fopen(opts.output_path.c_str(), "a");
    if (out_file == nullptr) {
      fprintf(stderr, "error: cannot open --output '%s'\n", opts.output_path.c_str());
      return 1;
    }
  }

  int exit_code = 0;
  for (const std::string &algorithm : opts.algorithms) {
    cache_t *cache = prefixsim::create_cache_by_name(algorithm, opts.cache_size);
    if (cache == nullptr) {
      fprintf(stderr, "error: unsupported algorithm '%s' (try --list-algos)\n",
              algorithm.c_str());
      exit_code = 1;
      continue;
    }

    prefixsim::SimulatorConfig config;
    config.cache_size_blocks = opts.cache_size;
    config.cost_model = opts.cost_model;
    config.verify = opts.verify;

    {
      prefixsim::Simulator sim(cache, config);
      if (!sim.run(requests, error)) {
        fprintf(stderr, "error [%s]: %s\n", algorithm.c_str(), error.c_str());
        exit_code = 1;
      } else {
        const prefixsim::Stats &stats = sim.stats();
        printf("%-24s block hit ratio %.4f   compute saving %.4f\n", algorithm.c_str(),
               stats.block_hit_ratio(), stats.compute_saving_ratio());
        if (stats.n_unexpected_evictions != 0) {
          fprintf(stderr,
                  "warning [%s]: %lld evictions happened during replay; the "
                  "allocation accounting and this algorithm disagree\n",
                  algorithm.c_str(),
                  static_cast<long long>(stats.n_unexpected_evictions));
        }
        report(stdout, algorithm, opts, stats);
        if (out_file != nullptr) report(out_file, algorithm, opts, stats);
      }
    }  // Simulator detaches its recorder here, before the cache is freed.

    cache->cache_free(cache);
  }

  if (out_file != nullptr) fclose(out_file);
  return exit_code;
}

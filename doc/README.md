# libCacheSim documentation

New here? Read these in order:

1. [Install and build](install.md): get the tools built.
2. [cachesim](quickstart_cachesim.md): run your first simulations and compare algorithms.
3. [Trace analysis](quickstart_traceAnalyzer.md): understand the workload behind the numbers.

Commands that start with `./bin/` are run from the build directory, `_build/`, which is why the sample traces appear as `../data/`.

## Command-line tools

- [cachesim](quickstart_cachesim.md): simulate caches, compare eviction algorithms, and sweep cache sizes.
- [traceAnalyzer](quickstart_traceAnalyzer.md): measure and plot request rate, object size, reuse, and popularity.
- [mrcProfiler](quickstart_mrcProfiler.md): build miss ratio curves for very large traces with sampling.
- [Trace utilities](quickstart_traceUtils.md): print, convert, sample, and filter traces.

## Traces

- [Synthetic traces](synthetic_traces.md): generate uniform or Zipf workloads without a trace file.
- [Oracle traces and zero-size requests](oracle_traces.md): what to check before trusting Belady results.
- [Open-source cache datasets](https://github.com/cacheMon/cache_dataset): real traces to experiment with.
- [lcp format](https://github.com/cacheMon/PrefixSim/blob/main/doc/lcp_format.md): LLM prefix-cache traces, used by [PrefixSim](https://github.com/cacheMon/PrefixSim).

## Writing your own algorithm or simulator

- [Plugins](quickstart_plugin.md): try a new eviction algorithm in C/C++ or Python without changing libCacheSim.
- [Python binding](https://github.com/cacheMon/libCacheSim-python): the easiest way to script simulations.
- [Using the library](advanced_lib.md): build your own simulator in C/C++.
- [Extending libCacheSim](advanced_lib_extend.md): add an eviction algorithm or a trace reader to the library.
- [C API reference](API.md).

## Performance and debugging

- [Performance tuning](performance.md)
- [Memory usage profiling](memory_usage_profiling.md)
- [Debugging](debug.md)

## Project

- [Contributing](/CONTRIBUTING.md)
- [Adoption census](adoption.md): who outside the project uses libCacheSim, with sources.

## Getting help

- [FAQ](/FAQ.md)
- [Discussions](https://github.com/1a1a11a/libCacheSim/discussions) for questions
- [Issue tracker](https://github.com/1a1a11a/libCacheSim/issues) for bugs

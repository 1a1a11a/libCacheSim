# libCacheSim

libCacheSim helps you answer questions about caches with real data: how big should my cache be, which eviction algorithm works best for my workload, and what does my workload look like?

It comes in three parts:

* **cachesim**, a command-line cache simulator. No code needed.
* **traceAnalyzer**, a command-line tool that describes a trace.
* **libCacheSim**, the C library underneath, for building your own simulators.

For LLM prefix caching, [PrefixSim](https://github.com/cacheMon/PrefixSim) builds a prefix-cache simulator on libCacheSim.

**New here?** Start with [Install and build](install.md), then [the cachesim guide](quickstart_cachesim.md).

**A note on paths.** Commands that run a built tool, like `./bin/cachesim` or `./bin/traceAnalyzer`, are run from the build directory (`_build/` if you followed the [install guide](install.md)). That's why the sample traces in `data/` appear as `../data/`. Each page says so if it uses a different directory.

```{toctree}
:maxdepth: 2
:caption: Getting started

install
quickstart_cachesim
quickstart_traceAnalyzer
quickstart_traceUtils
quickstart_mrcProfiler
quickstart_plugin
```

```{toctree}
:maxdepth: 2
:caption: Traces

synthetic_traces
oracle_traces
```

```{toctree}
:maxdepth: 2
:caption: Using libCacheSim as a library

advanced_lib
advanced_lib_extend
API
```

```{toctree}
:maxdepth: 2
:caption: Performance and debugging

performance
memory_usage_profiling
debug
```

## Other resources

* [Python binding](https://github.com/cacheMon/libCacheSim-python): `pip install libcachesim` for the easiest way to script simulations
* [FAQ](https://github.com/1a1a11a/libCacheSim/blob/develop/FAQ.md)
* [Contributing](https://github.com/1a1a11a/libCacheSim/blob/develop/CONTRIBUTING.md)
* [Open-source cache datasets](https://github.com/cacheMon/cache_dataset)
* [Issue tracker](https://github.com/1a1a11a/libCacheSim/issues) and [Discussions](https://github.com/1a1a11a/libCacheSim/discussions)

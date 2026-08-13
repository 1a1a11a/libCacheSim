# libCacheSim

A high-performance library for building and running cache simulations.

libCacheSim ships three things:

* **cachesim**, a high-performance cache simulator for running cache simulations.
* **traceAnalyzer**, a high-performance and versatile analyzer for cache traces.
* **libCacheSim**, a library for building your own cache simulators.

New here? Start with [Install & Build](install.md), then [the cachesim guide](quickstart_cachesim.md).

The commands throughout these pages are run from the build directory (`_build/` if you followed the [README](https://github.com/1a1a11a/libCacheSim#build-and-install-libcachesim)), so the sample traces in `data/` are at `../data/`.

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

* [Python binding](https://github.com/cacheMon/libCacheSim-python) — easier API access, `pip install libcachesim`
* [FAQ](https://github.com/1a1a11a/libCacheSim/blob/develop/FAQ.md)
* [Contributing](https://github.com/1a1a11a/libCacheSim/blob/develop/CONTRIBUTING.md)
* [Open-source cache datasets](https://github.com/cacheMon/cache_dataset)
* [Issue tracker](https://github.com/1a1a11a/libCacheSim/issues) and [Discussions](https://github.com/1a1a11a/libCacheSim/discussions)

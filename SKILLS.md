# libCacheSim Skills for Agents

What you can do in this repository. Each section maps to a concrete capability, the tool or API that provides it, and the commands to invoke it.

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Produces three binaries in `build/bin/`: `cachesim`, `traceAnalyzer`, `mrcProfiler`.  
Optional features require XGBoost/LightGBM; omit them unless the algorithm specifically needs it.

Install dependencies first if not present:

```bash
bash scripts/install_dependency.sh
```

---

## Simulate a cache

`cachesim` runs one or more eviction algorithms against a trace file and reports hit/miss ratios.

```bash
# Single algorithm, one cache size
./build/bin/cachesim trace.oracleGeneral oracleGeneral 1073741824 LRU

# Multiple algorithms in parallel
./build/bin/cachesim trace.oracleGeneral oracleGeneral 1073741824 LRU,S3-FIFO,Sieve

# Auto-detect working set, sweep a range of sizes
./build/bin/cachesim trace.oracleGeneral oracleGeneral 0 LRU --num-thread 4

# CSV trace with custom column mapping
./build/bin/cachesim trace.csv csv 1073741824 LRU \
  -t "time-col=0, obj-id-col=1, obj-size-col=2, delimiter=,"
```

**Supported eviction algorithms** (pass by name):  
FIFO, LRU, Clock, SLRU, LFU, LFUDA, ARC, TwoQ, CLOCK-PRO, Belady, BeladySize, GDSF,  
Hyperbolic, LeCaR, Cacheus, LHD, LRB, GLCache, WTinyLFU, 3LCache, QD-LP, S3-FIFO, Sieve

**Admission algorithms** (combine with `-a`): Adaptsize, Bloomfilter, Prob, Size

Full parameter reference: `./build/bin/cachesim --help`

---

## Analyze a trace

`traceAnalyzer` extracts statistics and generates plot-ready data from a trace.

```bash
# Overall statistics
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task stat

# Request rate over time (window = 300 s)
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task reqRate -w 300

# Object size distribution
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task size

# Reuse distance / reuse time distribution
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task reuse

# Popularity (frequency distribution)
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task popularity

# Run all analysis tasks and write output files for plotting
./build/bin/traceAnalyzer trace.oracleGeneral oracleGeneral --task all
```

Output files are written to the current directory and consumed by scripts in `scripts/traceAnalysis/`.

---

## Profile a miss ratio curve (MRC)

`mrcProfiler` builds an MRC without simulating every cache size from scratch.

```bash
# SHARDS sampler — fast, LRU only
./build/bin/mrcProfiler trace.oracleGeneral oracleGeneral SHARDS

# MINISIM sampler — supports any algorithm, multi-threaded
./build/bin/mrcProfiler trace.oracleGeneral oracleGeneral MINISIM --num-thread 8

# MRC relative to working set size
./build/bin/mrcProfiler trace.oracleGeneral oracleGeneral SHARDS --use-wss
```

SHARDS achieves ~23× speedup over full replay at <0.1% MAE.  
MINISIM supports all algorithms that `cachesim` supports.

---

## Plot results

Python scripts in `scripts/` consume output from `traceAnalyzer` and `mrcProfiler`.

```bash
# Miss ratio curve vs cache size
python3 scripts/plot_mrc_size.py --input result.mrc

# MRC over time
python3 scripts/plot_mrc_time.py --input result.mrc

# Approximate MRC (SHARDS / MINISIM output)
python3 scripts/plot_appr_mrc.py --input result.mrc

# Trace-level plots (run after traceAnalyzer --task all)
python3 scripts/traceAnalysis/access_pattern.py
python3 scripts/traceAnalysis/req_rate.py
python3 scripts/traceAnalysis/popularity.py
python3 scripts/traceAnalysis/reuse.py
```

---

## Generate a synthetic trace

```bash
# Zipf workload: 1M requests, 100K objects, alpha=1.0
python3 scripts/data_gen.py --num-req 1000000 --num-obj 100000 --alpha 1.0 \
  --output synthetic.oracleGeneral
```

---

## Inspect or convert a trace

```bash
# Print trace as human-readable text
./build/bin/tracePrint trace.oracleGeneral oracleGeneral | head -20

# Convert between trace formats
./build/bin/traceConv trace.csv csv trace.oracleGeneral oracleGeneral
```

---

## Use the C API in code

The library API lets you build custom simulators. See `example/` for complete working programs.

```c
#include "libCacheSim.h"

// Open a trace
reader_t *reader = open_trace("trace.oracleGeneral", ORACLE_GENERAL_TRACE, NULL);

// Create a cache (e.g. 1 GiB LRU)
common_cache_params_t params = default_common_cache_params();
params.cache_size = 1 * GiB;
cache_t *cache = LRU_init(params, NULL);

// Simulate
request_t *req = new_request();
while (read_one_req(reader, req) == 0) {
    cache->get(cache, req);  // returns true on hit
}

free_request(req);
free_cache(cache);
close_trace(reader);
```

**Multi-size simulation** (runs N cache sizes in one pass):

```c
simulate_at_multi_sizes(reader, cache, n_sizes, cache_sizes, NULL, 0, 4 /*threads*/);
```

See `example/cacheSimulator/` for a minimal single-cache example and  
`example/cacheSimulatorConcurrent/` for multi-size and CSV trace usage.

---

## Add a new eviction algorithm

1. Create `libCacheSim/cache/eviction/MyAlgo.c` — implement `init`, `get`, `find`, `insert`, `evict`, `remove`, `free`.
2. Register it in `libCacheSim/cache/cacheObj.c` and the algo name map.
3. Add it to `CMakeLists.txt`.
4. Rebuild and verify with `cachesim`.

See `doc/advanced_lib_extend.md` for the full walkthrough and `doc/advanced_lib.md` for internal data structures.

---

## Write a plugin (without modifying core)

The plugin system lets you implement an algorithm as a shared library loaded at runtime.

```bash
cd example/plugin_v2
cmake -S . -B build && cmake --build build
./build/test_plugin trace.oracleGeneral oracleGeneral 1073741824
```

Implement the six hooks in your plugin: `plugin_init`, `plugin_hit`, `plugin_miss`, `plugin_eviction`, `plugin_remove`, `plugin_free`. See `example/plugin_v2/myPlugin.cpp`.

---

## Run tests

```bash
cd build && ctest --output-on-failure
```

---

## Trace formats

| Format | Description |
|---|---|
| `oracleGeneral` | Binary struct: `{uint32 timestamp, uint64 obj_id, uint32 obj_size, int64 next_access_vtime}`. Supports `.zst` compression. |
| `csv` | Plain text; column mapping required via `-t` flag. |
| `binary` | Raw fixed-width binary records. |

Object IDs in oracleGeneral are hashed values. `next_access_vtime` is logical time (request count, not wall clock).

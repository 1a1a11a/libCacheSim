# Performance tuning

libCacheSim is built for high-throughput trace replay. This page collects the knobs that matter and how to measure their effect on your own workload — the numbers depend heavily on the trace, the algorithm, and the machine, so measure rather than assume.

## Measuring throughput

`cachesim` reports throughput (in millions of requests per second) on every run:

```bash
cd _build
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
```

For a systematic comparison across algorithms and working set sizes, [`scripts/benchmark_throughput.py`](/scripts/benchmark_throughput.py) generates Zipfian traces and sweeps them:

```bash
cd scripts
python3 benchmark_throughput.py --help
```

The sample traces in [`data/`](/data/) are far too small to benchmark with — they fit in cache and are dominated by startup cost. Use a trace of at least a few million requests.

## Build configuration

Build in release mode. A debug build is several times slower, and [`scripts/debug.sh`](/scripts/debug.sh) additionally skips tcmalloc to stay debugger-friendly:

```bash
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release
```

**tcmalloc** is linked automatically when CMake finds it, and matters because the hot path allocates per-object metadata. `cmake` prints `!!! cannot find tcmalloc` when it is missing; install it (`libgoogle-perftools-dev` on Debian/Ubuntu, `gperftools` via Homebrew) and reconfigure.

**Transparent hugepages** are enabled at compile time by default (`USE_HUGEPAGE=ON`), which reduces TLB misses on the hash table. They also need to be enabled on the host:

```bash
echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
```

Turn the compile-time option off with `-DUSE_HUGEPAGE=OFF` if your environment does not support them.

## Trace format

Binary formats are several times faster than csv, because csv parsing dominates replay for fast algorithms. Convert once with `traceConv` and reuse:

```bash
./bin/traceConv ../data/cloudPhysicsIO.csv csv \
    -t "time-col=2,obj-id-col=5,obj-size-col=4,obj-id-is-num=1" \
    --output-format=oracleGeneral
```

See [quickstart_traceUtils.md](quickstart_traceUtils.md). zstd-compressed binary traces are read without decompressing first, so compression costs little replay time while saving substantial disk.

When the id column holds numbers, pass `obj-id-is-num=true` so the reader skips hashing.

## Runtime options

* `--num-thread=N` — simulations across algorithms and cache sizes are run in parallel, so a sweep costs little more than its slowest single run.
* `--ignore-obj-size 1` — treats every object as size one. Faster, and the right choice when you want object miss ratio rather than byte miss ratio.
* `--consider-obj-metadata=false` — skips accounting for per-algorithm metadata overhead in the cache size.
* `--num-req=N` — caps how much of the trace is read, useful when iterating.

## Memory

Memory is dominated by the hash table and per-object metadata, so it scales with the number of *objects* rather than the number of requests. `--ignore-obj-size 1` with a small cache size keeps the object count down.

To profile actual usage, see [memory_usage_profiling.md](memory_usage_profiling.md).

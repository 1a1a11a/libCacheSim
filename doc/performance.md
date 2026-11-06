# Performance tuning

libCacheSim is built to replay traces fast. This page lists the settings that make the biggest difference and shows how to measure them. Results depend a lot on the trace, the algorithm and the machine, so measure on your own workload instead of trusting a single number.

The short version: use a release build with tcmalloc, convert your trace to a binary format, and let `cachesim` run your simulations in parallel.

## Measure first

`cachesim` prints its throughput at the end of every single-cache run, in MQPS (millions of requests per second):

```bash
cd _build
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
# ... miss ratio 0.6297, byte miss ratio 0.7274, throughput 4.62 MQPS
```

Don't benchmark with the sample traces in [`data/`](/data/). They are tiny, so startup cost dominates. Use a trace with at least a few million requests.

To compare algorithms and working set sizes systematically, [`scripts/benchmark_throughput.py`](/scripts/benchmark_throughput.py) generates Zipf traces and sweeps over them:

```bash
cd scripts
python3 benchmark_throughput.py --help
```

## Build for speed

**Use a release build.** Release is the default when you don't set `CMAKE_BUILD_TYPE`, so a plain build is already fine. To be explicit:

```bash
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release
ninja -C _build
```

A debug build is several times slower. The one [`scripts/debug.sh`](/scripts/debug.sh) makes also leaves out tcmalloc, so never measure with it.

**Make sure tcmalloc is linked.** The hot path allocates metadata for every cached object, and tcmalloc makes those allocations much cheaper. CMake links it automatically when it finds it. If it can't, the configure step prints:

```text
!!! cannot find tcmalloc
```

In that case, install it (`libgoogle-perftools-dev` on Debian/Ubuntu, `gperftools` on Homebrew) and run `cmake` again.

**Transparent hugepages** cut TLB misses on the large hash table. They are enabled at compile time by default (`USE_HUGEPAGE=ON`), but the host has to allow them too:

```bash
echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
```

If your environment doesn't support hugepages, build with `-DUSE_HUGEPAGE=OFF`.

## Use a binary trace

Parsing csv is often the slowest part of a run, especially with fast algorithms like FIFO or LRU. Binary formats are several times faster and use less memory. Convert once with `traceConv` and reuse the result:

```bash
./bin/traceConv ../data/cloudPhysicsIO.csv csv \
    -t "time-col=2,obj-id-col=5,obj-size-col=4,obj-id-is-num=1" \
    --output-format=oracleGeneral
```

libCacheSim reads zstd-compressed binary traces directly, so you can compress them to save disk space at little cost to replay speed. See [quickstart_traceUtils.md](quickstart_traceUtils.md) for more on conversion.

If you do stay with csv and the object id column holds numbers, pass `obj-id-is-num=true` so the reader doesn't have to hash every id.

## Runtime options

| Option | Why it helps |
| --- | --- |
| `--num-thread=N` | Simulations for different algorithms and cache sizes run in parallel, so a whole sweep takes about as long as its slowest run. |
| `--ignore-obj-size 1` | Treats every object as size one. This is faster, and it's the right choice when you care about object miss ratio rather than byte miss ratio. |
| `--consider-obj-metadata=false` | Skips counting each algorithm's per-object metadata against the cache size. |
| `--num-req=N` | Reads only the first N requests. Handy while you are iterating. |
| `--hashpower=N` | Sets the hash table size to 2^N entries (default 24). A smaller table uses less memory on small traces. Sampling-based algorithms pick eviction candidates from the hash table, so keep this fixed when you compare their results. |

## Memory

Memory use is mostly the hash table plus per-object metadata. It grows with the number of distinct *objects*, not the number of requests. A smaller cache, `--ignore-obj-size 1` or a lower `--hashpower` all keep it down.

To see where memory actually goes, see [memory_usage_profiling.md](memory_usage_profiling.md).

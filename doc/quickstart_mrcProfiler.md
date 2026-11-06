# Profile miss ratio curves with mrcProfiler

A *miss ratio curve* (MRC) shows how a cache's miss ratio changes as the cache grows. You can build one with `cachesim` by simulating many cache sizes, but on a large trace that takes a long time. `mrcProfiler` gets you a close estimate much faster by simulating only a sample of the trace.

It offers two profilers:

| Profiler | Algorithms | How it samples | Notes |
| --- | --- | --- | --- |
| **SHARDS** | LRU only | A fixed sampling rate, or a fixed number of sampled objects | Very fast. Fixed-size mode caps memory use. |
| **MINISIM** (miniature simulation) | Any algorithm `cachesim` supports | A fixed sampling rate | Replays a scaled-down copy of the trace for each cache size, in parallel threads. |

Both report the request miss ratio and the byte miss ratio together. Cache sizes can be absolute (`100MB`) or fractions of the *working set size* (WSS), which is the total size of all distinct objects in the trace.

How good is the estimate? On the 1.3-billion-request `cluster52.oracleGeneral.sample10` trace, SHARDS at a 1% sampling rate builds an LRU curve in about 20 seconds with a mean absolute error below 0.1%. Simulating the same sizes with `cachesim` takes 472 seconds. Details are in [Accuracy and speed](#accuracy-and-speed) below.

All commands run from the build directory, `_build/`, so the sample traces are at `../data/`.

## Quick start

Profile LRU on the sample trace at five sizes, from 10% to 50% of the working set:

```bash
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,5
```

```text
profiler: SHARDS
trace: ../data/cloudPhysicsIO.vscsi
cache_algorithm: LRU
n_req: 113872
sum_obj_size_req: 4205978112
wss_ratio	cache_size	miss_rate	byte_miss_rate
0.100000	202976972B	0.817585	0.956895
0.200000	405953945B	0.729767	0.875030
0.300000	608930918B	0.705178	0.847945
0.400000	811907891B	0.643705	0.751716
0.500000	1014884864B	0.641949	0.748405
```

Each row is one cache size. When you give sizes as fractions, the first column is that fraction of the working set. The sample trace is tiny, so treat these numbers as a smoke test, not a result.

## The command line

```text
./bin/mrcProfiler <trace_path> <trace_type> --algo=<algorithm> --profiler=<SHARDS|MINISIM> \
    --profiler-params=<params> --size=<sizes> [--ignore-obj-size]
```

| Option | Meaning |
| --- | --- |
| `--algo` | Eviction algorithm. SHARDS only supports `LRU`. MINISIM accepts any name `cachesim` accepts. |
| `--profiler` | `SHARDS` or `MINISIM` |
| `--profiler-params` | See [Profiler parameters](#profiler-parameters) |
| `--size` | See [Choose the cache sizes](#choose-the-cache-sizes) |
| `--ignore-obj-size` | Treat every object as size 1, so sizes count objects instead of bytes |
| `-t` | Trace reader options for csv traces, the same as in `cachesim` |
| `-n` | Only read the first N requests |

Run `./bin/mrcProfiler --help` for everything else. To plot the results, use [scripts/profile_mrc.py](/scripts/profile_mrc.py); see the [scripts README](/scripts/README.md).

### Profiler parameters

`--profiler-params` is a comma-separated triple:

| Value | Meaning |
| --- | --- |
| `FIX_RATE,0.01,42` | SHARDS, sample 1% of objects, using hash salt (random seed) 42 |
| `FIX_SIZE,8192,42` | SHARDS, sample a fixed 8192 objects, using hash salt 42. Memory stays bounded no matter how large the trace is. |
| `FIX_RATE,0.01,10` | MINISIM, sample 1% of objects, using 10 threads |

For SHARDS the third value is a seed, and different seeds give slightly different curves. For MINISIM it is the thread count. A MINISIM sampling rate above 0.5 turns sampling off and replays the full trace.

### Choose the cache sizes

`--size` accepts either a range or a list:

* **A range**: `start,end,points` gives `points` evenly spaced sizes from `start` to `end`. Three values whose last one is a whole number greater than 1 are read as a range.
* **A list**: any other comma-separated sizes are used as given.

Each size is either absolute (`10MB`, `1GiB`) or a fraction of the working set (`0.1`).

```bash
# 10 sizes from 100MB to 1GB
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=100MB,1GB,10

# 10 sizes from 10% to 50% of the working set
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10

# exactly these fractions of the working set
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=0.01,0.02,0.04,0.08,0.16

# exactly these absolute sizes
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=10MB,20MB,40MB,80MB,160MB
```

## Profile LRU with SHARDS

SHARDS hashes each object id and keeps an object only if its hash falls in the sampled range. It then measures LRU behavior on the sampled objects and scales the result.

```bash
# fixed sampling rate: 1% of objects
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_RATE,0.01,42 --size=100MB,1GB,10

# fixed sample size: track at most 2048 objects
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS \
    --profiler-params=FIX_SIZE,2048,42 --size=100MB,1GB,10
```

## Profile other algorithms with MINISIM

MINISIM also samples objects (spatial sampling), then replays the smaller trace through a scaled-down cache for each size. Because it really runs the algorithm, it works for FIFO, ARC, S3-FIFO, Sieve, 2Q and the rest. See the [README](/README.md#supported-algorithms) for the full list.

```bash
# FIFO, 1% sampling rate, 10 threads
./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=FIFO --profiler=MINISIM \
    --profiler-params=FIX_RATE,0.01,10 --size=0.1,0.5,10
```

### Belady and BeladySize

`belady` and `beladySize` need to know when each object is requested next, so they only run on an oracle trace format such as `oracleGeneral` or `lcs`. On any other trace, the profiler stops with an error that tells you so. Convert your trace with [traceConv](quickstart_traceUtils.md) first.

`beladySize` is also less accurate under sampling than other algorithms, and mrcProfiler prints a warning when you use it. The reason is a mismatch in how distance is measured. It ranks objects by how far away their next request is, computed as `next_access_vtime - n_req`. But `next_access_vtime` counts requests in the full trace, while `n_req` counts only the sampled requests, so the distance comes out too large.

On `cloudPhysicsIO` at a 100 MB cache with a sampling rate of 0.5, `beladySize` lands 0.0126 away from the unsampled miss ratio. `belady` is off by only 0.0003, and LRU by 0.0023. If you need an exact `beladySize` curve, turn sampling off with `FIX_RATE,1,<threads>`. Or use `belady`, which compares next-access times directly and is not affected.

## Other trace formats

mrcProfiler reads every trace format `cachesim` reads, with the same `-t` options for csv traces. See the [cachesim guide](quickstart_cachesim.md).

## Accuracy and speed

The maintainers measured both profilers against full `cachesim` runs.

* **Trace**: `cluster52.oracleGeneral.sample10` (about 1.3 billion requests)
* **Machine**: AMD EPYC 7K62 48-core processor, 64 GB memory
* **Cache sizes**: 10 sizes from 10 MB to 100 MB
* **Error**: mean absolute error (MAE) of the miss ratio, compared with `cachesim`

### SHARDS compared with cachesim (LRU)

| Tool | Parameters | Time | Error (MAE) |
| --- | --- | --- | --- |
| cachesim | full simulation, 10 threads | 472 s | — |
| SHARDS | `FIX_RATE,0.01` | 21 s | 0.083% |
| SHARDS | `FIX_SIZE,32768` | 26 s | 0.076% |

<details>
<summary>Commands used</summary>

```bash
# cachesim
time ./bin/cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral LRU 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# SHARDS with a 0.01 sampling rate
time ./bin/mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10

# SHARDS with a fixed sample of 32768 objects
time ./bin/mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=LRU --profiler=SHARDS --profiler-params=FIX_SIZE,32768,10 --size=10MB,100MB,10
```

</details>

### MINISIM compared with cachesim

| Tool | Algorithm | Parameters | Time | Error (MAE) |
| --- | --- | --- | --- | --- |
| cachesim | FIFO | full simulation, 10 threads | 241 s | — |
| cachesim | ARC | full simulation, 10 threads | 357 s | — |
| cachesim | S3FIFO | full simulation, 10 threads | 359 s | — |
| MINISIM | FIFO | `FIX_RATE,0.01,10` | 76 s | 0.24% |
| MINISIM | ARC | `FIX_RATE,0.01,10` | 77 s | 0.48% |
| MINISIM | S3FIFO | `FIX_RATE,0.01,10` | 81 s | 0.15% |

<details>
<summary>Commands used</summary>

```bash
# cachesim
time ./bin/cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral FIFO 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0
time ./bin/cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral ARC 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0
time ./bin/cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral S3FIFO 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# MINISIM with a 0.01 sampling rate and 10 threads
time ./bin/mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=FIFO --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10
time ./bin/mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=ARC --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10
time ./bin/mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=S3FIFO --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10
```

</details>

## Roadmap

Done: the profiler command line, the SHARDS and miniature simulation profilers, accuracy and speed tests, and plotting scripts.

Planned: more profilers, such as FLOWS, TTLs Matter, Kosmo and AET.

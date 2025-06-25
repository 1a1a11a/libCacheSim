# mrcProfiler user guide 
mrcProfiler is a tool provided by libCacheSim to quickly profile Miss Ratio Curves (MRCs) for large-scale workloads. It currently supports:
* **SHARDS profiler** for both fixed sampling rate and fixed sample size modes. It only supports the LRU algorithm.
* **MINISIM profiler** for fixed sampling rate mode. It supports non-LRU eviction algorithms such as LFU, 2Q, FIFO, and more.
* MRC profiling based on **working set size (WSS)** or **fixed cache sizes**.
* Simultaneous construction of **MRC** and **Byte-MRC**.
* Multi-threaded simulations for the **MINISIM profiler**. 

With proper configuration, mrcProfiler can achieve highly accurate MRCs with minimal overhead. For example, the **SHARDS profiler** with a `0.01` sampling rate can generate an MRC for `cluster52.oracleGeneral.sample10` trace with 1.3 billion requests in approximately 20 seconds, with an Mean Absolute Error (MAE) below 0.1%. The time overhead of trace replay for single cache size is 472 seconds, reduced by a factor of 23.

---

## Installation
First, [build libCacheSim](/doc/install.md). After building libCacheSim, `mrcProfiler` should be in the build directory. 

---

## Basic Usage

```
./mrcProfiler trace_path trace_type --algo=[LRU] --profiler=[SHARDS|MINISIM]
            --profiler-params=[FIX_RATE,0.01,hash_salt|FIX_SIZE,8192,hash_salt|FIX_RATE,0.01,thread_num(for MINISIM)]
            --size=[0.01,1,100|1MiB,100MiB,100|0.001,0.002,0.004,0.008,0.016|1MiB,10MiB,10MiB,1GiB]
```

Use ./mrcProfiler --help for more details.

Plot scripts are provided in `scripts/profile_mrc.py`. See [here](/scripts/README.md) for more details.

### Profiling LRU with SHARDS

Run the example `vscsi` trace with SHARDS profiler. 
SHARDS is configured in `fixed sampling rate` mode with a sampling rate of `0.01` and a random seed of `42` (different seeds may yield varying results).

The cache sizes for MRC generation are specified in `fixed-size mode`, spanning `10` evenly spaced points from `100MB` to `1GB`:

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=100MB,1GB,10
```

SHARDS can also operate in `fixed sample size` mode, limiting memory usage by sampling a fixed number of unique objects. The example below samples `2048` objects:

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_SIZE,2048,42 --size=100MB,1GB,10
```

### Profiling MRC with WSS-Based Sizes

Generate an MRC based on WSS percentages. The example below creates `10` evenly spaced points from `10%` to `50%` of the WSS:

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10
```

### Profiling MRC with Specific Sizes

mrcProfiler supports both `WSS-based` and `fixed-size` MRC generation for specific sizes. Examples:


**WSS-based sizes:**

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.01,0.02,0.04,0.08,0.16
```

**Fixed cache sizes:**

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=10MB,20MB,40MB,80MB,160MB
```


### Profiling Non-LRU Algorithms with MINISIM

**Miniature Simulation** uses spatial sampling to downsample the workload and estimates miss ratios via scaled-down replay. It supports **non-LRU** algorithms.
In the example below, `FIX_RATE,0.01,10` sets a `1%` sampling rate and `10` threads. Note: Sampling rates above 0.5 disable sampling (full trace replay).

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=FIFO --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=0.1,0.5,10
```

### Ignoring Object Sizes

To ignore object sizes (treat all objects as 1-byte):

```bash
./mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10 --ignore-obj-size
```

### Supporting Different Trace Formats

mrcProfiler supports various trace formats. For details, refer to [quickstart_cachesim](/doc/quickstart_cachesim.md).


## Performance and Accuracy

* trace: `cluster52.oracleGeneral.sample10`
* CPU: AMD EPYC 7K62 48-Core Processor
* Memory:  64GB
* metric: MAE & time

### SHARDS VS cachesim

Commands:

```bash
# cachesim
time ./cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral LRU 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# mrcProfiler with SHARDS with 0.01 sample rate
time ./mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10

# mrcProfiler with SHARDS with 8192 sample size
time ./mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=LRU --profiler=SHARDS --profiler-params=FIX_SIZE,32768,10 --size=10MB,100MB,10
```

resluts:

| Profiler |Algorithm| Parameters     | Time       | Error (MAE) |
|----------|---------|----------------|------------|-------------|
| cachesim | LRU     | -              | 472 seconds with 10 thread| - |
| SHARDS   | LRU     |FIX_RATE,0.01   | 21 seconds | 0.083%     |
| SHARDS   | LRU     |FIX_SIZE,32768  | 26 seconds | 0.076%     |

### MINISIM VS cachesim

Commands:

```bash
# cachesim for FIFO
time ./cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral FIFO 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# cachesim for ARC
time ./cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral ARC 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# cachesim for S3FIFO
time ./cachesim /path_to/cluster52.oracleGeneral.sample10 oracleGeneral S3FIFO 10MB,20MB,30MB,40MB,50MB,60MB,70MB,80MB,90MB,100MB --verbose=0

# mrcProfiler for FIFO eviction algorithm with MINISIM with 0.01 sample rate
time ./mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=FIFO --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10

# mrcProfiler for ARC eviction algorithm with MINISIM with 0.01 sample rate
time ./mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=ARC --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10

# mrcProfiler for S3FIFO eviction algorithm with MINISIM with 0.01 sample rate
time ./mrcProfiler /path_to/cluster52.oracleGeneral.sample10 oracleGeneral --algo=S3FIFO --profiler=MINISIM --profiler-params=FIX_RATE,0.01,10 --size=10MB,100MB,10
```

resluts:

| Profiler |Algorithm| Parameters     | Time       | Error (MAE) |
|----------|---------|----------------|------------|-------------|
| cachesim | FIFO    | -              | 241 seconds with 10 thread| - |
| cachesim | ARC     | -              | 357 seconds with 10 thread| - |
| cachesim | S3FIFO  | -              | 359 seconds with 10 thread| - |
| MINISIM  | FIFO    | FIX_RATE,0.01,10 | 76 seconds with 10 thread| 0.24% |
| MINISIM  | ARC     | FIX_RATE,0.01,10 | 77 seconds with 10 thread| 0.48% |
| MINISIM  | S3FIFO  | FIX_RATE,0.01,10 | 81 seconds with 10 thread| 0.15% |


## Roadmap

- [x] Add mrcProfiler with core parameter parsing.

- [x] Implement SHARDS and Miniature Simulation profilers.

- [x] Conduct performance and accuracy tests.

- [x] Integrate plotting scripts.

- [ ] Implement additional profilers (e.g., FLOWS, TTLs Matter, Kosmo, AET).

# cachesim user guide
cachesim is a tool provided by libCacheSim to quickly run some cache simulations, it supports
* a variety of eviction algorithms such as FIFO, LRU, LFU, ARC, SLRU, LeCaR, CACHEUS, Hyperbolic, LHD, TinyLFU, Belady, LRB and GLCache.
* a variety of admission algorithms such as size, bloomFilter and adaptSize.
* text, csv trace as well as binary traces.
* automatic multi-threaded simulations.

Meanwhile, cachesim has high-performance with low resource usages.

---

## Installation
First, [build libCacheSim](/doc/install.md). After building libCacheSim, `cachesim` is in the `bin/` subdirectory of your build directory.

All commands on this page are run from the build directory (`_build/` if you followed the [README](/README.md)), so the sample traces in [data/](/data/) are at `../data/`.

---

## Basic Usage
```
./bin/cachesim trace_path trace_type eviction_algo cache_size [OPTION...]
```

use `./bin/cachesim --help` to get more information.

### Run a single cache simulation

Run the example vscsi trace with LRU eviction algorithm and 1GB cache size.
Note that vscsi is a trace format, we also support csv traces.

```bash
# Note that no space between the cache size and the unit, unit is not case sensitive
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
```

### Run multiple cache simulations
```bash
# Note that there is no space between the cache sizes
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1mb,16mb,256mb,8gb

# Or you can quote the cache sizes
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru "1mb, 16mb, 256mb, 8gb"

# besides absolute cache size, you can also use fraction of working set size
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 0.001,0.01,0.1,0.2

# besides using byte as the unit, you can also treat all objects having the same size, and the size is the number of objects
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1000,16000 --ignore-obj-size 1

# new feature: you can run a few algorithms in parallel by concatenating the algorithms
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi fifo,lru,arc,qdlp 0.01 --ignore-obj-size 1

# run 4*4 simulations in parallel (no more than n_thread at the same time)
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi fifo,lru,arc,qdlp 0.01,0.05,0.1,0.2 --ignore-obj-size 1
```


### Auto detect cache sizes
cachesim can detect the working set of the trace and automatically generate cache sizes at 0.001, 0.003, 0.01, 0.03, 0.1, 0.2, 0.4, 0.8 of the working set size.
You can enable this feature by setting cache size to 0 or auto.

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru auto
```

### Use different eviction algorithms
cachesim supports the following algorithms:
* [FIFO](/libCacheSim/cache/eviction/FIFO.c)
* [LRU](/libCacheSim/cache/eviction/LRU.c)
* [Clock](/libCacheSim/cache/eviction/Clock.c)
* [LFU](/libCacheSim/cache/eviction/LFU.c)
* [ARC](/libCacheSim/cache/eviction/ARC.c)
* [SLRU](/libCacheSim/cache/eviction/SLRU.c)
* [GDSF](/libCacheSim/cache/eviction/cpp/GDSF.cpp)
* [WTinyLFU](/libCacheSim/cache/eviction/WTinyLFU.c)
* [LeCaR](/libCacheSim/cache/eviction/LeCaR.c)
* [Cacheus](/libCacheSim/cache/eviction/Cacheus.c)
* [Hyperbolic](/libCacheSim/cache/eviction/Hyperbolic.c)
* [LHD](/libCacheSim/cache/eviction/LHD/LHD_Interface.cpp)
* [Belady](/libCacheSim/cache/eviction/Belady.c)
* [BeladySize](/libCacheSim/cache/eviction/BeladySize.c)
* [QD-LP](/libCacheSim/cache/eviction/QDLP.c)
* [S3-FIFO](/libCacheSim/cache/eviction/S3FIFO.c), [Sieve](/libCacheSim/cache/eviction/Sieve.c)
* [GLCache](/libCacheSim/cache/eviction/GLCache/GLCache.c) — build with `-DENABLE_GLCACHE=ON`

See the [README](/README.md#supported-algorithms) for the full list, including the algorithms that are behind an optional build flag (GLCache, LRB, 3LCache). Asking for one that was not compiled in fails with `do not support algorithm <name>`.

You can just use the algorithm name as the eviction algorithm parameter, for example

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lecar auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi hyperbolic auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lhd auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi s3fifo auto

# belady and beladySize require oracle trace
./bin/cachesim ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral beladySize auto
```


### Use different trace types
We have demonstrated the use of cachesim with vscsi trace. We also support csv traces.
To use a csv trace, we need to provide the column of *time*, *obj-id*, and *obj-size*.
Both time and size are optional, but many algorithms rely on time and size to work properly.
The column starts from 1, the first column is 1, the second is 2, etc.
Besides the column information, a csv reader also requires the delimiter and whether the csv file has a header.
cachesim builds in a simple delimiter and header detector, if the detected result is not correct, you can provide the correct information using `delimiter=,`, `has-header=true`.


Object ids are hashed unless you tell the reader they are already numeric, so add `obj-id-is-num=true` when the id column holds numbers — `cachesim` stops with an error if you leave it out on such a trace. The sample `cloudPhysicsIO.csv` has a numeric id column, so every example below sets it.

```bash
# note that the parameters are separated by comma and quoted
./bin/cachesim ../data/cloudPhysicsIO.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true"

# omitting obj-id-is-num on a numeric id column fails with
#   [ERROR] csv.c: detect obj_id is numeric, please specify -t 'obj-id-is-num=1'

# note that csv trace does not support UTF-8 encoding, only ASCII encoding is supported
./bin/cachesim ../data/cloudPhysicsIO.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true, delimiter=,, has-header=true"
```

Besides csv trace, we also support txt trace and binary trace.
```bash
# txt trace is a simple format that stores obj-id in each line
./bin/cachesim ../data/cloudPhysicsIO.txt txt lru 1gb

# binary trace, format is specified using format string similar to Python struct
./bin/cachesim ../data/cloudPhysicsIO.vscsi binary lru 1gb -t "format=<IIIHHQQ,obj-id-col=6,obj-size-col=2"

# oracleGeneral is a binary format that stores time, obj-id, size, next-access-time (in reference count)
./bin/cachesim ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral lru 1gb
```
**We recommend using binary trace because it can be a few times faster than csv trace and uses less DRAM resources.**



## Advanced usage

cachesim supports many advanced features, you can use `./bin/cachesim --help` to get more information.
Here we give some examples.

### Setting parameters for eviction algorithms
Some eviction algorithms have parameters, you can set the parameters by using `-e "k1=v1,k2=v2"` or `--eviction-params "k1=v1,k2=v2"` format.
```bash
# run SLRU with 4 segments
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi slru 1gb -e n-seg=4

# print the default parameters for SLRU
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi slru 1gb -e print
```


### Admission algorithm
cachesim supports the following admission algorithms: size, probabilistic, bloomFilter, adaptSize.
You can use `-a` or `--admission` to set the admission algorithm.
```bash
# add a bloom filter to filter out objects on first access
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -a bloomFilter
```

### Prefetching algorithm
cachesim supports the following prefetching algorithms: OBL, Mithril, PG (and AMP is on the way).
You can use `-p` or `--prefetch` to set the prefetching algorithm.
```bash
# add a mithril to record object association information and fetch objects that are likely to be accessed in the future
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -p Mithril
```

### Advanced features
```bash
# change number of threads
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --num-thread=4

# cap the number of requests read from the trace
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --num-req=1000000

# change output
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -o my-output

# ignore object size, each object has size one
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --ignore-obj-size=true

# ignore object metadata size, different algorithms have different metadata size, this option will ignore the metadata size
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --consider-obj-metadata=false

# use part of the trace to warm up the cache
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --warmup-sec=86400

# Use TTL
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --use-ttl=true

# Disable the print of the first few requests
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --print-head-req=false
```

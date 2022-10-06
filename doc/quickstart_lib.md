

# libCacheSim library user guide 
libCacheSim is a library to build fast cache simulators. 
With its high-throughput, low-footprint, flexible API design, libCacheSim can simulate large caches within seconds to minutes.
Some example usages of libCacheSim 
* simulate a cache with a given eviction policy, e.g., LRU, LFU, ARC, SLRU, Hyperbolic, LHD, LeCaR, GLCache, etc. 
* design a new cache admission, eviction algorithm. 
* 
* build a cache hierarchy with multiple layers to study the impact of exclusive and inclusive caching.
* build a cache cluster with consistent hashing. 

## 

Meanwhile, cachesim has high-performance with low resource usages. 

---

## core APIs
First, [build libCacheSim](../README.md). After building libCacheSim, `cachesim` should be in the build directory. 


## examples 
### Design a new cache eviction algorithm
libCacheSim supports developing new algorithms in C and C++, however, for the best performance, C is preferred.

add the header file to [evictionALgo.h](../libCacheSim/include/libCacheSim/evictionAlgo.h) and implement the functions. 
add the source file to [CMakeLists.txt](../libCacheSim/cache/eviction/CMakeLists.txt) 
add the algorithm to [cachesim](../libCacheSim/bin/cachesim/cli.c) 


#### C example


#### C++ example
and implement the functions.


### Build a cache hierarchy with multiple layers


### Build a cache cluster with consistent hashing





---

## Basic Usage
```
./cachesim trace_path trace_type eviction_algo cache_size [OPTION...]
```

use `./cachesim --help` to get more information.

### Run a single cache simulation

Run the example vscsi trace with LRU eviction algorithm and 1GB cache size. 
Note that vscsi is a trace format, we also support csv traces. 

```bash
# Note that no space between the cache size and the unit, unit is not case sensitive
./cachesim ../data/trace.vscsi vscsi lru 1gb 
```

### Run multiple cache simulations
```bash
# Note that there is no space between the cache sizes
./cachesim ../data/trace.vscsi vscsi lru 1mb,16mb,256mb,8gb

# Or you can quote the cache sizes
./cachesim ../data/trace.vscsi vscsi lru "1mb, 16mb, 256mb, 8gb"
``` 

### Auto detect cache sizes
cachesim can detect the working set of the trace and automatically generate cache sizes at 0.0001, 0.0003, 0.001, 0.003, 0.01, 0.03, 0.1, 0.3 of the working set size. 
You can enable this feature by setting cache size to 0 or auto.

```bash
./cachesim ../data/trace.vscsi vscsi lru auto
```

### Use different eviction algorithms
cachesim supports the following algorithms:
[FIFO](../libCacheSim/libCacheSim/cache/eviction/FIFO.c), 
[LRU](../libCacheSim/libCacheSim/cache/eviction/LRU.c), 
[Clock](../libCacheSim/libCacheSim/cache/eviction/Clock.c),
[LFU](../libCacheSim/libCacheSim/cache/eviction/LFU.c), 
[ARC](../libCacheSim/libCacheSim/cache/eviction/ARC.c), 
[SLRU](../libCacheSim/libCacheSim/cache/eviction/SLRU.c), 
[GDSF](../libCacheSim/libCacheSim/cache/eviction/GDSF.c), 
[TinyLFU](../libCacheSim/libCacheSim/cache/eviction/TinyLFU.c), 
[LeCaR](../libCacheSim/libCacheSim/cache/eviction/LeCaR.c), 
[Cacheus](../libCacheSim/libCacheSim/cache/eviction/Cacheus.c), 
[Hyperbolic](../libCacheSim/libCacheSim/cache/eviction/Hyperbolic.c), 
[LHD](../libCacheSim/libCacheSim/cache/eviction/LHD/LHDInterface.cpp), 
[GLCache](../libCacheSim/libCacheSim/cache/eviction/GLCache/GLCache.c),
[Belady](../libCacheSim/libCacheSim/cache/eviction/Belady.c), 
[BeladySize](../libCacheSim/libCacheSim/cache/eviction/BeladySize.c), 

You can just use the algorithm name as the eviction algorithm parameter, for example  

```bash
./cachesim ../data/trace.vscsi vscsi lecar auto
./cachesim ../data/trace.vscsi vscsi hyperbolic auto
./cachesim ../data/trace.vscsi vscsi lhd auto
./cachesim ../data/trace.vscsi vscsi glcache auto

# belady and beladySize require oracle trace
./cachesim ../data/trace.oracleGeneral oracleGeneral beladySize auto
```


### Use different trace types 
We have demonstrated the use of cachesim with vscsi trace. We also support csv traces.
To use a csv trace, we need to provide the column of *time*, *obj-id*, and *obj-size*. 
Both time and size are optional, but many algorithms rely on time and size to work properly.
The column starts from 1, the first column is 1, the second is 2, etc.
Besides the column information, a csv reader also requires the delimiter and whether the csv file has a header. 
cachesim builds in a simple delimiter and header detector, if the detected result is not correct, you can provide the correct information using `dlimiter=,`, `has-header=true`.


```bash
# note that the parameters are separated by comma and quoted
./cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4"

# if object id is numeric, then we can pass obj-id-is-num=true to speed up
./cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true"


# note that csv trace does not support UTF-8 encoding, only ASCII encoding is supported
./cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, delimiter=,, has-header=true"
```

Besides csv trace, we also support txt trace and binary trace. 
```bash
# txt trace is a simple format that stores obj-id in each line
./cachesim ../data/trace.txt txt lru 1gb

# binary trace, format is specified using format string similar to Python struct
./cachesim ../data/trace.vscsi binary lru 1gb -t "format=<IIIHHQQ,obj-id-col=6,obj-size-col=2"

# oracleGeneral is a binary format that stores time, obj-id, size, next-access-time (in reference count)
./cachesim ../data/trace.oracleGeneral.bin oracleGeneral lru 1gb
```
**We recommend using binary trace because it can be a few times faster than csv trace and uses less DRAM resources.**



## Advanced usage

cachesim supports many advanced features, you can use `./cachesim --help` to get more information.
Here we give some examples. 

### Setting parameters for eviction algorithms
Some eviction algorithms have parameters, you can set the parameters by using `-e "k1=v1,k2=v2"` or `--eviction-params "k1=v1,k2=v2"` format.
```bash
# run SLRU with 4 segments
./cachesim ../data/trace.vscsi vscsi slru 1gb -e n-seg=4

# print the default parameters for SLRU
./cachesim ../data/trace.vscsi vscsi slru 1gb -e print
```


### Admission algorithm
cachesim supports the following admission algorithms: size, probabilistic, bloomFilter, adaptSize.
You can use `-a` or `--admission-algo` to set the admission algorithm. 
```bash
# add a bloom filter to filter out objects on first access
./cachesim ../data/trace.vscsi vscsi lru 1gb -a bloomFilter
```


### Advanced features 
```bash
# change number of threads 
./cachesim ../data/trace.vscsi vscsi lru 1gb --num-thread=4

# cap the number of requests raed from the trace
./cachesim ../data/trace.vscsi vscsi lru 1gb --num-req=1000000

# change output 
./cachesim ../data/trace.vscsi vscsi lru 1gb -o my-output

# ignore object size, each object has size one
./cachesim ../data/trace.vscsi vscsi lru 1gb --ignore-obj-size=true

# ignore object metadata size, different algorithms have different metadata size, this option will ignore the metadata size
./cachesim ../data/trace.vscsi vscsi lru 1gb --consider-obj-metadata=false

# use part of the trace to warm up the cache
./cachesim ../data/trace.vscsi vscsi lru 1gb --warmup-sec=86400

# Use TTL
./cachesim ../data/trace.vscsi vscsi lru 1gb --use-ttl=true

```




## Other trace utilities
We also provide some trace utilities to help you use the traces and debug applications. 

### tracePrint
Print requests from a trace.

```bash
# print 10 requests from a trace
./tracePrint ../data/trace.vscsi vscsi -n 10
```

### traceStat


### traceGen


### traceConv
Convert a trace to libCacheSim format so it has a smaller size and runs faster. 
```bash
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin
```

We can also sample a trace to reduce the size of the trace. 
```bash
# sample 1% of the trace
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin -s 0.01
```


### traceFilter

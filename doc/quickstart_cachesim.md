
# cachesim user guide 
cachesim is a tool provided by libCacheSim to quick run some cache simulations, it supports 
* a variety of eviction algorithms such as FIFO, LRU, LFU, ARC, SLRU, LeCaR, Cacheus, Hyperbolic, LHD, TinyLFU, Belady, and GLCache. 
* a variety of admission algorithms such as size, bloomFilter and adaptSize. 
* text, csv trace as well as binary traces. 
* automatic multi-threaded evaluations. 

Meanwhile, cachesim has high-performance with low resource usages. 

---

## Installation
First, [build libCacheSim](../README.md). After building libCacheSim, `cachesim` should be in the build directory. 

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
```


### Use different trace types 
We have demonstrated the use of cachesim with vscsi trace. We also support csv traces.
To use a csv trace, we need to provide the column of *time*, *obj_id*, and *obj_size*. 
Both time and size are optional, but many algorithms rely on time and size to work properly.
The column starts from 1, the first column is 1, the second is 2, etc.
Besides the column information, a csv reader also requires the delimiter and whether the csv file has a header. 
cachesim builds in a simple delimiter and header detector, if the detected result is not correct, you can provide the correct information using `dlimiter=,`, `has_header=true`.


```bash
# note that the parameters are separated by colon and quoted
./cachesim ../data/trace.csv csv lru 1gb

# note that csv trace does not support UTF-8 encoding, only ASCII encoding is supported
# because we separate parameters by colon, we do not support using colon as the csv delimiter


# oracleGeneral is a binary format that stores time, obj_id, size, next_access_time (in reference count)
./cachesim ../data/trace.oracleGeneral.bin oracleGeneral lru 1gb

```



## Advanced usage

cachesim supports many advanced features, you can use `./cachesim --help` to get more information.
Here we give some examples. 

### Setting parameters for eviction algorithms
Some eviction algorithms have parameters, you can set the parameters by using `-e k1=v1;k2=v2` or `--eviction_params k1=v1;k2=v2` format.
```bash
# run SLRU with 4 segments
./cachesim ../data/trace.vscsi vscsi slru 1gb -e n_seg=4

# print the default parameters for SLRU
./cachesim ../data/trace.vscsi vscsi slru 1gb -e print
```


### Admission algorithm
cachesim supports the following admission algorithms: size, probabilistic, bloomFilter, adaptSize.
You can use `-a` or `--admission_algo` to set the admission algorithm. 
```bash
# add a bloom filter to filter out objects on first access
./cachesim ../data/trace.vscsi vscsi lru 1gb -a bloomFilter
```


### Advanced features 
```bash
# change output 
./cachesim ../data/trace.vscsi vscsi lru 1gb -o my_output

# ignore object size, each object has size one
./cachesim ../data/trace.vscsi vscsi lru 1gb --ignore_obj_size=true

# ignore object metadata size, different algorithms have different metadata size, this option will ignore the metadata size
./cachesim ../data/trace.vscsi vscsi lru 1gb --consider_obj_metadata=false

# use part of the trace to warm up the cache
./cachesim ../data/trace.vscsi vscsi lru 1gb --warmup_sec=86400

# Use TTL
./cachesim ../data/trace.vscsi vscsi lru 1gb --use_ttl=true

```





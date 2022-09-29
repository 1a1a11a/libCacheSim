
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

You can just use the algorithm name as the eviction algorithm parameter. 

```bash
./cachesim ../data/trace.vscsi vscsi lecar auto
./cachesim ../data/trace.vscsi vscsi hyperbolic auto
./cachesim ../data/trace.vscsi vscsi lhd auto
./cachesim ../data/trace.vscsi vscsi glcache auto
```


### Use different trace types 




## Advanced usage

### Ignore object size


### Ignore object metadata size 

### Use TTL

### Set parameters for eviction algorithms

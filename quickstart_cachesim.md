
## cachesim use guide 
cachesim is a tool provided by libCacheSim to quick run some cache simulations, it supports 
* a variety of eviction algorithms such as FIFO, LRU, LFU, ARC, SLRU, LeCaR, Cacheus, Hyperbolic, LHD, TinyLFU, Belady, and GLCache. 
* a variety of admission algorithms such as size, bloomFilter and adaptSize. 
* text, csv trace as well as binary traces. 
* automatic multi-threaded evaluations. 

Meanwhile, cachesim has high-performance with low resource usages. 

---

## Installation
After building libCacheSim, `cachesim` should be in the build directory. 

---

## Usage
```
./cachesim trace_path trace_type eviction_algo cache_size [OPTION...]
```

use `./cachesim --help` to get more information.

### Basic usage (run a single cache simulation)

```bash
./cachesim ../data/trace.vscsi vscsi lru 1gb 
```


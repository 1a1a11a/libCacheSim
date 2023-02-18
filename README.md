# libCacheSim - building and running cache simulations

<!-- [![Documentation Status](https://readthedocs.org/projects/libCacheSim/badge/?version=master)](http://libCacheSim.readthedocs.io/en/develop/?badge=master)
[![GitHub version](https://badge.fury.io/gh/1a1a11a%2FlibCasheSim.svg)](https://badge.fury.io/gh/1a1a11a%2FlibCasheSim) -->


[![build](https://github.com/1a1a11a/libCacheSimPrv/actions/workflows/build.yml/badge.svg)](https://github.com/1a1a11a/libCacheSimPrv/actions/workflows/build.yml)
![visitors](https://visitor-badge.glitch.me/badge?page_id=1a1a11a.libCacheSim)


## What is libCacheSim
* a high-performance **cache simulator** binary for running cache simulations. 
* a high-performance **library** for building cache simulators. 

---

## libCacheSim features 
* **high performance** - over 20M requests/sec for a realistic trace replay. 
* **high memory efficiency** - predictable and small memory footprint. 
* **State-of-the-art algorithms** - eviction algorithms, admission algorithms, sampling techniques, approximate miss ratio computation. 
* **Simple API** - easy to build cache clusters, multi-layer caching, etc.
* **extensible** - easy to support new trace types or eviction algorithms.
---

## Supported algorithms
cachesim supports the following algorithms:
* [FIFO](libCacheSim/cache/eviction/FIFO.c)
* [LRU](libCacheSim/cache/eviction/LRU.c)
* [Clock](libCacheSim/cache/eviction/Clock.c)
* [LFU](libCacheSim/cache/eviction/LFU.c)
* [LFU with dynamic aging](libCacheSim/cache/eviction/LFUDA.c)
* [ARC](libCacheSim/cache/eviction/ARC.c)
* [SLRU](libCacheSim/cache/eviction/SLRU.c)
* [GDSF](libCacheSim/cache/eviction/cpp/GDSF.cpp)
* [TinyLFU](libCacheSim/cache/eviction/TinyLFU.c)
* [LeCaR](libCacheSim/cache/eviction/LeCaR.c)
* [Cacheus](libCacheSim/cache/eviction/Cacheus.c)
* [Hyperbolic](libCacheSim/cache/eviction/Hyperbolic.c)
* [LHD](libCacheSim/cache/eviction/LHD/LHD_Interface.cpp)
* [LRB](libCacheSim/cache/eviction/LRB/LRB_Interface.cpp)
* [GLCache](libCacheSim/cache/eviction/GLCache/GLCache.c)
* [Belady](libCacheSim/cache/eviction/Belady.c)
* [BeladySize](libCacheSim/cache/eviction/BeladySize.c)

---


## Build and Install libCacheSim
### Install dependency
libCacheSim uses [camke](https://cmake.org/) build system and has a few dependencies: 
[glib](https://developer.gnome.org/glib/)
[tcmalloc](https://github.com/google/tcmalloc), 
[zstd](https://github.com/facebook/zstd).

Please see [install.md](doc/install.md) for how to install the dependencies. 


### Build libCacheSim
cmake recommends **out-of-source build**, so we do it in a new directory:
```
git clone https://github.com/1a1a11a/libCacheSimPrv
pushd libCachesimPrv;
mkdir _build && cd _build;
cmake .. && make -j;
[sudo] make install;
popd;
```
---

## Usage
### cachesim (a high-performance cache simulator)
After building and installing libCacheSim, `cachesim` should be in the build directory. 
#### basic usage
```
./cachesim trace_path trace_type eviction_algo cache_size [OPTION...]
```

use `./cachesim --help` to get more information.

#### Run a single cache simulation
Run the example traces with LRU eviction algorithm and 1GB cache size. 

```bash
# Note that no space between the cache size and the unit, unit is not case sensitive
./cachesim ../data/trace.vscsi vscsi lru 1gb 
```

#### Run multiple cache simulations with different cache sizes
```bash
# Note that no space between the cache sizes
./cachesim ../data/trace.vscsi vscsi lru 1mb,16mb,256mb,8gb

# besides absolute cache size, you can also use fraction of working set size
./cachesim ../data/trace.vscsi vscsi lru 0.001,0.01,0.1,0.2

# besides using byte as the unit, you can also treat all objects having the same size, and the size is the number of objects
./cachesim ../data/trace.vscsi vscsi lru 1000,16000 --ignore obj-size 1

# use a csv trace, note the qutation marks when you have multiple options
./cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4"

# use a csv trace with more options
./cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, delimiter=,, has-header=true"
``` 

See [quick start cachesim](doc/quickstart_cachesim.md) for more usages. 



---

### Using libCacheSim as a library 
libCacheSim can be used as a library for building cache simulators. 
For example, you can build a cache cluster with consistent hashing, or a multi-layer caching simulator.

Here is a simplified example showing the basic APIs. 
```c 
#include <libCacheSim.h>

/* open trace, see quickstart_lib.md for opening csv and binary trace */
reader_t *reader = open_trace("../data/trace.vscsi", VSCSI_TRACE, NULL);

/* craete a container for reading from trace */
request_t *req = new_request();

/* create a LRU cache */
common_cache_params_t cc_params = {.cache_size=1024*1024U}; 
cache_t *cache = LRU_init(cc_params, NULL); 

/* counters */
uint64_t req_byte = 0, miss_byte = 0;
uint64_t n_req = 0, n_miss = 0;

/* loop through the trace */
while (read_one_req(reader, req) == 0) {
    if (cache->get(cache, req) == cache_ck_miss) {
        miss_byte += req->obj_size;
        n_miss++;
    }
    req_byte += req->obj_size; 
    n_req++;
}

printf("miss ratio: %.4lf, byte miss ratio %.4lf\n", 
        (double)n_miss / n_req, (double)miss_byte / req_byte);

/* cleaning */
close_trace(reader);
free_request(req);
cache->cache_free(cache);
```
save this to `test.c` and compile with 
```
gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o test.out
```

if you get `error while loading shared libraries`, run `sudo ldconfig`

See [quickstart](doc/quickstart_lib.md) for more details. 
And see [example folder](example) for examples on how to use libCacheSim, such as cache cluster with consistent hashing, multi-layer caching simulators. 

---


### Extending libCacheSim 
#### Adding new trace types
libCacheSim supports txt, csv, and binary traces. We prefer binary traces because it allows libCacheSim to run faster, and the traces are more compact. 

We also support zstd compressed traces with decompression first, this allows you to store the traces with less space.

You should not need to add support to use a new trace if you follow the (cachesim quick start tutorial)[doc/quickstart_cachesim.md] and (libCacheSim quick start tutorial)[doc/quickstart_libcachesim].

But if you ever need to add a new trace type, please see `libCacheSim/traceReader/customizedReader/akamaiBin.h` for an example reader.

#### Adding new eviction algorithms
Adding eviction algorithm is easy, you can see (`libCacheSim/cache/eviction/LRU.c`)[libCacheSim/cache/eviction/LRU.c] for an example.
Besides implementing the a new eviction algorithm in `libCacheSim/cache/eviction/myCache.c`, you also need to perform the following tasks.
1. Add the `myCache_init()` function to (`libCacheSim/include/libCacheSim/evictionAlgo.h`)[libCacheSim/include/libCacheSim/evictionAlgo.h].
2. Add the mycache.c to (`libCacheSim/cache/eviction/CMakeLists.txt`)[libCacheSim/cache/eviction/CMakeLists.txt] so that it can be compiled.
3. Add the option to use mycache in `cachesim` in (`libCacheSim/bin/cachesim/cli.c`)[libCacheSim/bin/cachesim/cli.c].
4. If you are creating a pull request, you would also need to add a test in (`test/test_evictionAlgo.c`)[test/test_evictionAlgo.c] and add the algorithm to this README. 

#### Adding new eviction algorithms in C++
You can also write your eviction algorithm in C++ and use it in libCacheSim.
You can see `libCacheSim/cache/eviction/cpp/LFU.cpp` for an example.

For further reading on how to use libCacheSim, please see the [quick start libCacheSim](doc/quickstart_libcachesim.md).


---
### Questions? 
Please join the Google group https://groups.google.com/g/libcachesim and ask questions.


---  
### Contributions 
We gladly welcome pull requests.
Before making any changes, we recommend opening an issue and discussing your proposed changes.  
This will let us give you advice on the proposed changes. If the changes are minor, then feel free to make them without discussion. 
This project adheres to Google's coding style. By participating, you are expected to uphold this code. 

---
### Reference
```
@inproceedings {libCacheSim,
    author = {Juncheng Yang and Yao Yue and K. V. Rashmi},
    title = {A large scale analysis of hundreds of in-memory cache clusters at Twitter},
    booktitle = {14th USENIX Symposium on Operating Systems Design and Implementation (OSDI 20)},
    year = {2020},
    isbn = {978-1-939133-19-9},
    pages = {191--208},
    url = {https://www.usenix.org/conference/osdi20/presentation/yang},
    publisher = {USENIX Association},
    month = nov,
}
```
---

### License
See [LICENSE](LICENSE) for details.

### Related
* [PyMimircache](https://github.com/1a1a11a/PyMimircache): a python based cache trace analysis platform, now deprecated
---



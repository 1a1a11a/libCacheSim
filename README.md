# libCacheSim - building and running cache simulations

[![build](https://github.com/1a1a11a/libCacheSim/actions/workflows/build.yml/badge.svg)](https://github.com/1a1a11a/libCacheSim/actions/workflows/build.yml)

**The stable development of libCacheSim has moved to [https://github.com/cacheMon/libCacheSim](https://github.com/cacheMon/libCacheSim)**. 


## News
* **2023 June**: **QD-LP** is available now, see [our paper](https://dl.acm.org/doi/10.1145/3593856.3595887) for details.
* **2023 Oct**: **S3-FIFO** and **Sieve** are generally available! These are simple algorithms that are very effective in reducing cache misses. Try them out in libCacheSim and your production!
---

## What is libCacheSim
* a high-performance **cache simulator** for running cache simulations. 
* a high-performance and versatile trace analyzer for **analyzing different cache traces**.
* a high-performance **library** for building cache simulators. 

---

## libCacheSim features 
* **High performance** - over 20M requests/sec for a realistic trace replay. 
* **High memory efficiency** - predictable and small memory footprint. 
* **State-of-the-art algorithms** - eviction algorithms, admission algorithms, sampling techniques, approximate miss ratio computation, see [here](/doc/quickstart_cachesim.md). 
* Parallelism out-of-the-box - uses the many CPU cores to speed up trace analysis and cache simulations. 
* **The ONLY feature-rich trace analyzer** - all types of trace analysis you need, see [here](/doc/quickstart_traceAnalyzer.md).
* **Simple API** - easy to build cache clusters, multi-layer caching, etc, see [here](/doc/API.md).
* **Extensible** - easy to support new trace types or eviction algorithms, see [here](/doc/advanced_lib_extend.md).
---

## Supported algorithms
cachesim supports the following algorithms:
* [FIFO](/libCacheSim/cache/eviction/FIFO.c), [LRU](/libCacheSim/cache/eviction/LRU.c), [Clock](/libCacheSim/cache/eviction/Clock.c), [SLRU](/libCacheSim/cache/eviction/SLRU.c)
* [LFU](/libCacheSim/cache/eviction/LFU.c), [LFU with dynamic aging](/libCacheSim/cache/eviction/LFUDA.c)
* [ARC](/libCacheSim/cache/eviction/ARC.c), [TwoQ](/libCacheSim/cache/eviction/TwoQ.c)
* [Belady](/libCacheSim/cache/eviction/Belady.c), [BeladySize](/libCacheSim/cache/eviction/BeladySize.c)
* [GDSF](/libCacheSim/cache/eviction/cpp/GDSF.cpp)
* [Hyperbolic](/libCacheSim/cache/eviction/Hyperbolic.c)
* [LeCaR](/libCacheSim/cache/eviction/LeCaR.c)
* [Cacheus](/libCacheSim/cache/eviction/Cacheus.c)
* [LHD](/libCacheSim/cache/eviction/LHD/LHD_Interface.cpp)
* [LRB](/libCacheSim/cache/eviction/LRB/LRB_Interface.cpp)
* [GLCache](/libCacheSim/cache/eviction/GLCache/GLCache.c)
* [TinyLFU](/libCacheSim/cache/eviction/TinyLFU.c)
* [QD-LP](/libCacheSim/cache/eviction/QDLP.c)
* [S3-FIFO](/libCacheSim/cache/eviction/S3FIFO.c)
* [Sieve](/libCacheSim/cache/eviction/Sieve.c)
---


## Build and Install libCacheSim
### One-line install
We provide some scripts for quick installation of libCacheSim. 
```bash
cd scripts && bash install_dependency.sh && bash install_libcachesim.sh;
```
If this does not work, please 
1. let us know what system you are using and what error you get
2. read the following sections for self-installation.

### Install dependency
libCacheSim uses [cmake](https://cmake.org/) build system and has a few dependencies: [glib](https://developer.gnome.org/glib/), [tcmalloc](https://github.com/google/tcmalloc), [zstd](https://github.com/facebook/zstd).
Please see [install.md](/doc/install.md) for how to install the dependencies. 


### Build libCacheSim
cmake recommends **out-of-source build**, so we do it in a new directory:
```
git clone https://github.com/1a1a11a/libCacheSim
pushd libCachesim;
mkdir _build && cd _build;
cmake .. && make -j;
[sudo] make install;
popd;
```
---

## Usage
### cachesim (a high-performance cache simulator)
After building and installing libCacheSim, `cachesim` should be in the `_build/bin/` directory. 
#### basic usage
```
./bin/cachesim trace_path trace_type eviction_algo cache_size [OPTION...]
```

use `./bin/cachesim --help` to get more information.

#### Run a single cache simulation
Run the example traces with LRU eviction algorithm and 1GB cache size. 

```bash
# Note that no space between the cache size and the unit, unit is not case sensitive
./bin/cachesim ../data/trace.vscsi vscsi lru 1gb 
```

#### Run multiple cache simulations with different cache sizes
```bash
# Note that no space between the cache sizes
./bin/cachesim ../data/trace.vscsi vscsi lru 1mb,16mb,256mb,8gb

# besides absolute cache size, you can also use fraction of working set size
./bin/cachesim ../data/trace.vscsi vscsi lru 0.001,0.01,0.1,0.2

# besides using byte as the unit, you can also treat all objects having the same size, and the size is the number of objects
./bin/cachesim ../data/trace.vscsi vscsi lru 1000,16000 --ignore-obj-size 1

# use a csv trace, note the qutation marks when you have multiple options
./bin/cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4"

# use a csv trace with more options
./bin/cachesim ../data/trace.csv csv lru 1gb -t "time-col=2, obj-id-col=5, obj-size-col=4, delimiter=,, has-header=true"
``` 

See [quick start cachesim](/doc/quickstart_cachesim.md) for more usages. 


#### Plot miss ratio curve
You can plot miss ratios of different algorithms and sizes, and plot the miss ratios over time.

```bash
# plot miss ratio over size
cd scripts;
python3 plot_mrc_size.py --tracepath ../data/twitter_cluster52.csv --trace-format csv --trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=," --algos=fifo,lru,lecar,s3fifo --sizes=0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.2,0.3,0.4

# plot miss ratio over time
python3 plot_mrc_time.py --tracepath ../data/twitter_cluster52.csv --trace-format csv --trace-format-params="time-col=1, obj-id-col=2, obj-size-col=3, delimiter=,," --algos=fifo,lru,lecar,s3fifo --report-interval=30 --miss-ratio-type="accu"
```

---

### Trace analysis
libCacheSim also has a trace analyzer that provides a lot of useful information about the trace. 
And it is very fast, designed to work with billions of requests. 
It also coms with a set of scripts to help you analyze the trace. 
See [trace analysis](/doc/quickstart_traceAnalyzer.md) for more details.




---

### Using libCacheSim as a library 
libCacheSim can be used as a library for building cache simulators. 
For example, you can build a cache cluster with consistent hashing, or a multi-layer cache simulator.

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
uint64_t n_req = 0, n_miss = 0;

/* loop through the trace */
while (read_one_req(reader, req) == 0) {
    if (!cache->get(cache, req)) {
        n_miss++;
    }
    n_req++;
}

printf("miss ratio: %.4lf\n", (double)n_miss / n_req);

/* cleaning */
close_trace(reader);
free_request(req);
cache->cache_free(cache);
```

save this to `test.c` and compile with 
``` bash
gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o test.out
```

See [here](/doc/advanced_lib.md) for more details, and see [example folder](/example) for examples on how to use libCacheSim, such as building a cache cluster with consistent hashing, multi-layer cache simulators. 

---


### Extending libCacheSim (new algorithms and trace types)
libCacheSim supports *txt*, *csv*, and *binary* traces. We prefer binary traces because it allows libCacheSim to run faster, and the traces are more compact. 

We also support zstd compressed binary traces without decompression. This allows you to store the traces with less space.

If you need to add a new trace type or a new algorithm, please see [here](/doc/advanced_lib_extend.md) for details. 


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
@inproceedings{yang20-workload,
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

@inproceedings{yang2023-s3fifo,
  title={FIFO queues are all you need for cache eviction},
  author={Yang, Juncheng and Zhang, Yazhuo and Qiu, Ziyue and Yue, Yao and Rashmi, K.V.},
  booktitle={Symposium on Operating Systems Principles (SOSP'23)},
  year={2023}
}
```
If you used libCacheSim in your research, please cite the above papers. And we welcome you to send us a link to your paper and add reference to (references.md)[references.md].

---


### License
See [LICENSE](LICENSE) for details.

### Related
* [PyMimircache](https://github.com/1a1a11a/PyMimircache): a python based cache trace analysis platform, now deprecated
---



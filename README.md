## libCacheSim - a library for cache simulation, profiling, and analysis 


![Build Status](https://travis-ci.com/1a1a11a/libCacheSim.svg?token=yJEqB6qLmWucGFp9zK4U&branch=master)
[![Documentation Status](https://readthedocs.org/projects/libCacheSim/badge/?version=master)](http://libCacheSim.readthedocs.io/en/develop/?badge=master)
[![GitHub version](https://badge.fury.io/gh/1a1a11a%2FlibCasheSim.svg)](https://badge.fury.io/gh/1a1a11a%2FlibCasheSim)
![visitors](https://visitor-badge.glitch.me/badge?page_id=1a1a11a.libCacheSim)


### What is libCacheSim
* a high-performance library for building cache simulators. 
* a cache trace profiler supporting fast LRU miss ratio/reuse distance computation. 
* a tool for gathering statistics of cache traces. 
---

### libCacheSim features 
* [**high performance**](performance.md) - over 20M requests/sec for a realistic trace replay. 
* [**high memory efficiency**](performance.md) - predictable and small memory footprint ~36 bytes memory per cached object. 
* [**performance and memory mode**](performance.md) - run the simulator in performance mode with larger memory footprint, or run it in slower low-memory mode.  
* **State-of-the-art algorithms** - eviction algorithms, admission algorithms, sampling techniques, approximate miss ratio computation. 
* **Simple API** - easy to build cache clusters, multi-layer caching, etc.
* **extensible** - easy to add traceReader or eviction algorithms either in source or load using plugin systems.    
* **Realistic simulator** - support object size, wall clock time, operations, etc. 
* **wide trace formats support** - support txt, csv/tsv, binary, vscsi trace formats. 
---

### Build and Install 
libCacheSim uses [camke](https://cmake.org/) build system and has two dependencies - 
[GNOME glib](https://developer.gnome.org/glib/) and [Google tcmalloc](https://github.com/google/tcmalloc).

#### Install dependency
Mac (using [homebrew](https://brew.sh/) as an example)
```
brew install glib google-perftools
```

Linux (using Ubuntu as an example)
```
sudo apt install libglib2.0-dev libgoogle-perftools-dev
```

#### Build libCacheSim
cmake recommends **out-of-source build**, so we do it in a new directory:
```
git clone https://github.com/1a1a11a/libCacheSim 
mkdir _build
cd _build
cmake ..
make -j
[sudo] make install
```

#### Linking with libCacheSim
linking can be done in cmake or use pkg-config 

#### Performance Optimizations 
* hugepage - to turn on hugepage support, please do `echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled`


---
### Quickstart  
See [example folder](example) for more examples on how to use libCacheSim, such as cache cluster with consistent hashing, multi-layer caching simulators. 
Here is a simplified example showing the most basic APIs. 
```c 
#include <libCacheSim.h>

/* open trace, see quickstart.md for opening csv and binary trace */
reader_t *reader = open_trace("data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);

/* craete a container for reading from trace */
request_t *req = new_request();

/* create a LRU cache */
common_cache_params_t cc_params = {.cache_size=1024*1024U}; 
cache_t *cache = create_cache("LRU", cc_params, NULL); 

/* counters */
uint64_t req_byte = 0, miss_byte = 0;

/* loop through the trace */
while (read_one_req(reader, req) == 0) {
    if (cache->get(cache, req) == cache_ck_miss) {
        miss_byte += req->obj_size;
    }
    req_byte += req->obj_size; 
}

/* cleaning */
close_trace(reader);
free_request(req);
cache->cache_free(cache);


```
save this to `test.c` and compile with 
```
gcc $(pkg-config --cflags --libs libCacheSim glib-2.0) -lm -ldl test.c -o test.out
```

if you get `error while loading shared libraries`, run `sudo ldconfig`



---
### Documentation 
* see [quick start](quickstart.md) for a quickstart tutorial. 
* see [API.md](API.md) for common APIs.  
* see [http://mimircache.info](http://mimircache.info) for full doc. 
  
  



---
### Update and Roadmap
* June 2020: v0.1 finalized APIs, performance tuning, ready for internal use 
* Aug 2020:  v0.2 alpha version for public 

  
---  
### Contributions 
```
We gladly welcome pull requests.
Before making any changes, we recommend opening an issue and discussing your proposed changes.  
This will let us give you advice on the proposed changes. If the changes are minor, then feel free to make them without discussion. 
This project adheres to Google's coding style. By participating, you are expected to uphold this code. 
```

---
#### Reference
Fill in

---

---
#### Related
* [PyMimircache](https://github.com/1a1a11a/PyMimircache): a python based cache trace analysis platform
---



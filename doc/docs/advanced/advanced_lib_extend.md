# Extend libCacheSim 


## Add new eviction algorithms in C
Implementing a new eviction algorithm is easy in libCacheSim. You can start from [LRU](/libCacheSim/cache/eviction/LRU.c) as the template, copy to `myCache.c` and change the functions. The cache interface requires the developer to implement the following functions: 

```c
/* initialize all the variables */
cache_t *LRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);

/* free the resources used by the cache */
void LRU_free(cache_t *cache);

/* get the object from the cache, it is find + on-demand insert/evict, return true if cache hit */
bool LRU_get(cache_t *cache, const request_t *req);

/* find an object in the cache, return the cache object if found, NULL otherwise, update_cache means whether update the cache state, e.g., moving object to the head of the queue */
cache_obj_t *LRU_find(cache_t *cache, const request_t *req, const bool update_cache);

/* insert an object to the cache, return the cache object, this assumes the object is not in the cache */
cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);

/* find the object to be evicted, return the cache object, not used very often */
cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req);

/* evict an object from the cache, req should not be used */
void LRU_evict(cache_t *cache, const request_t *req);

/* remove an object from the cache, return true if the object is found and removed, note that this is used for user-triggered remove, eviction should use evict */
bool LRU_remove(cache_t *cache, const obj_id_t obj_id);
```

Specifically, you can following the steps:
1. Add a new file e.g., `mycache.c` to [cache/eviction/](/libCacheSim/cache/eviction/) for your cache eviction algorithm implementation. 
2. If your cache eviction algorithm needs extra metadata, add a new object metadata struct in 
   [include/libCacheSim/cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h).
3. Add `myCache_init()` function to [include/libCacheSim/evictionAlgo.h](/libCacheSim/include/libCacheSim/evictionAlgo.h).
4. Add mycache.c to [CMakeLists.txt](/libCacheSim/cache/eviction/CMakeLists.txt) so that it can be compiled.
5. Add command line option in [bin/cachesim/cache_init.h](/libCacheSim/bin/cachesim/cache_init.h) so that you can use `cachesim` binary. You may also want to take a look at [bin/cachesim/cli_parser.c](/libCacheSim/bin/cachesim/cli_parser.c). 
6. Remember to add a test in [test/test_evictionAlgo.c](/test/test_evictionAlgo.c) and add the algorithm to this [README](README.md). 

> [!TIP]
> Many eviction algorithms use a doubly linked list to maintain state, libCacheSim provides several functions in [cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h) to manipulate list. 


> [!TIP]
> Many eviction algorithms are composable, e.g., LeCaR uses one LRU and one LFU, it is easy to support these algorithms in libCacheSim, please take a look at [LeCaR](/libCacheSim/cache/eviction/LeCaRv0.c). 


---

## Add new eviction algorithms in C++
While most of the eviction algorithms in libCacheSim is written in C, there are a few written in C++, especially the ones that are ported from the original user, e.g., [LHD](/libCacheSim/cache/eviction/LHD/), [LRB](/libCacheSim/cache/eviction/LRB/).

You can also write your eviction algorithm in C++ and use it in libCacheSim. We have provided a few C++ eviction algorithms as examples in [cache/eviction/cpp/](/libCacheSim/cache/eviction/cpp/), e.g., [LFU](/libCacheSim/cache/eviction/cpp/LFU.cpp), [LRU-K](/libCacheSim/cache/eviction/cpp/LRU_K.cpp).

There are two steps you can follow, 
1. implement most functions in C++ 
2. implement the libCacheSim cache interface in C, e.g., `mycache_init()`, `mycache_get()`. 



---

## Add new trace readers 
libCacheSim supports [txt](/libCacheSim/traceReader/generalReader/txt.c), [csv](/libCacheSim/traceReader/generalReader/csv.c), and binary traces. We prefer binary traces because it allows libCacheSim to run faster, and the traces are more compact. 
For binary traces, libCacheSim also supports zstd compressed traces without decompression.

But if you ever need to implement a new trace type, please see [here](/libCacheSim/traceReader/customizedReader/akamaiBin.h) for an example reader. 

To implement a reader, you need to implement two functions:
```c
/* initialize the reader, return 0 if success, 1 otherwise */
int myReader_setup(reader_t *reader);

/* read one request from the trace, return 0 if success, 1 otherwise */
int myReader_read_one_req(reader_t *reader, request_t *req);

```

Here are the steps to add a new trace reader:
1. add a new new trace type, e.g., `MYREADER_TRACE` in `trace_type_e` in [include/libCacheSim/enum.h](/libCacheSim/include/libCacheSim/enum.h). 
2. add a new reader file, e.g., `myReader.h` in [traceReader/customizedReader/](/libCacheSim/traceReader/customizedReader/) and implement the two functions.
3. add `myReader_setup()` to `setup_reader()`and `myReader_read_one_req()` to `read_one_req()` in [traceReader/reader.c](/libCacheSim/traceReader/reader.c). 
4. add `MYREADER_TRACE` to `trace_type_str_to_enum()` in [bin/cli_reader_utils.c](/libCacheSim/bin/cli_reader_utils.c)



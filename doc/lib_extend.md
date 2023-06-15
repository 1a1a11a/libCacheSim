# Extend libCacheSim 


## Add new eviction algorithms 
Adding eviction algorithm is easy. You can see `libCacheSim/cache/eviction/LRU.c` for an example.
You need to implement the following functions: 
```c
cache_t *LRU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);
void LRU_free(cache_t *cache);
bool LRU_get(cache_t *cache, const request_t *req);
cache_obj_t *LRU_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);
cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req);
void LRU_evict(cache_t *cache, const request_t *req);
bool LRU_remove(cache_t *cache, const obj_id_t obj_id);
```

Besides implementing the a new eviction algorithm in `libCacheSim/cache/eviction/myCache.c`, you also need to perform the following tasks.
1. Add `mycache.c` to `libCacheSim/cache/eviction/` for your cache eviction algorithm implementation. you may also have to add a new object metadata struct in `libCacheSim/include/libCacheSim/cacheObj.h`.
2. Add the `myCache_init()` function to `libCacheSim/include/libCacheSim/evictionAlgo.h`.
3. Add the mycache.c to `libCacheSim/cache/eviction/CMakeLists.txt` so that it can be compiled.
4. Add the option to use mycache in `cachesim` in `libCacheSim/bin/cachesim/cli.c`.
5. If you are creating a pull request, you would also need to add a test in `test/test_evictionAlgo.c` and add the algorithm to this README. 


## Adding new eviction algorithms in C++
You can also write your eviction algorithm in C++ and use it in libCacheSim.
You can see `libCacheSim/cache/eviction/cpp/LFU.cpp` for an example.



## Add new trace readers 
libCacheSim supports txt, csv, and binary traces. We prefer binary traces because it allows libCacheSim to run faster, and the traces are more compact. 

We also support zstd compressed traces with decompression first, this allows you to store the traces with less space.

You should not need to add support to use a new trace if you follow the (cachesim quick start tutorial)[doc/quickstart_cachesim.md] and (libCacheSim quick start tutorial)[doc/quickstart_libcachesim].

But if you ever need to add a new trace type, please see `libCacheSim/traceReader/customizedReader/akamaiBin.h` for an example reader.



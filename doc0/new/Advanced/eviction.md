### Eviction algorithms 

libCacheSim supports a variety cache eviction algorithms, such as LRU, FIFO, GDSF, LHD, LeCaR, LFU, LFU dynamic aging, ARC, SLRU, Random, etc. 




#### Implementing a new eviction algorithm 
Implementing a new cache eviction algorithm is easy in libCacheSim, currently we support implementation in C and C++. 

To implement an eviction algorithm, we need to implement the following functions. Take FIFO as an example, 


```C
cache_t *FIFO_init(common_cache_params_t ccache_params,
                   void *cache_specific_params);

void FIFO_free(cache_t *cache);

bool FIFO_check(cache_t *cache, request_t *req, bool update_cache);

bool FIFO_get(cache_t *cache, request_t *req);

void FIFO_insert(cache_t *FIFO, request_t *req);

void FIFO_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

void FIFO_remove_obj(cache_t *cache, cache_obj_t *cache_obj);

```



Detailed examples can be found under `libCacheSim/evictionAlgo/`. 
files ending with `.c` are implemented in ANSI C, files ending with `.cpp` are implemented using C++11. 

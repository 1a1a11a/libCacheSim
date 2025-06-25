# libCacheSim library user guide 
libCacheSim is a library to build fast cache simulators. 
With its high-throughput, low-footprint, flexible API design, libCacheSim can simulate large caches within seconds to minutes.
Some example usages of libCacheSim 
* design/compare cache admission/eviction algorithms. 
* design/compare trace sampling techniques.
* simulate a complex cache hierarchy and topology. 
For example, build a cache hierarchy with multiple layers to study the impact of exclusive and inclusive caching; or build a cache cluster with consistent hashing to explore the different load balancing techniques on caching. 


---

## libCacheSim Modules 
Currently there are four main modules in libCacheSim, 
* **bin**: a set of binary tools including cachesim, traceConv, etc.
* **traceReader**: a module providing trace parsing and reading, currently supports csv, txt, and binary traces. 
* **cache**: it includes three modules --- eviction, admission and prefetching. 
  * **eviction**: provides a set of cache eviction algorithms such as LRU, LFU, FIFO, CLOCK, ARC, LFUDA (LFU with dynamic aging), SLRU, Hyperbolic, LHD, LeCaR, Cacheus, GLCache, etc. 
  * **admission**: provides a set of admission algorithms including size, bloomFilter, adaptSize.
  * **prefetch**: provides various prefetch algorithms, currently it is not used. 

---

## APIs
### Cache APIs
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

#### Create Cache
We can create a cache by calling the cache initialization function, such as for LRU 
```c
cache_t *cache = LRU_init(common_cache_params, NULL); 
```
The common_cache_params is a `common_cache_params_t` struct, the hashpower is the estimated size of hash table, default is 24, which means default hash table size is `1 << 24` (16777216) objects. Note that setting an appropriate
```c
typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int hashpower;
} common_cache_params_t;
```
Note that setting an appropriate hashpower can reduce the number of times hash table expands, but only set it if you know what you doing, otherwise, leave it as the default. 

#### Close/free the cache
```c
// this will call the corresponding free function
cache->cache_free(cache);
```

#### Cache get
This is the highest level cache API, which tries to find the object in the cache. If found, it updates the cache state (e.g., moving the object to the head of queue in LRU); otherwise, it inserts the object in the cache, and evict if necessary. 
```c
// req is the request, see the next section for reading from a trace
cache->get(cache, req);
```

The APIs below is lower level API, if you use the lower level API, you need to explicitly update the `n_req` field of the cache after each user request, otherwise, some algorithms that rely on virtual time may not work correctly.

#### Cache find
This is the lower level cache API, which tries to find the object in the cache. If found, it updates the cache state if `update_cache` is true, and return the found object; otherwise, it returns NULL. 
```c
cache->find(cache, req, update_cache);
```

#### Cache insert
This is the lower level cache API, which tries to insert the object in the cache. If the object can be inserted into the cache, it returns the inserted object. Otherwise, it returns NULL. 

Note that there are cases that the object is not inserted into the cache, e.g., the object is too large to fit in the cache. 

Because this is the second level API, this function does not perform eviction and assumes the cache has enough space. 
If using this function, you need to explicitly call `evict` to evict objects. 
```c
cache->insert(cache, req);
```

#### Cache evict
This is the lower level cache API, which tries to evict the object in the cache. If the object can be evicted from the cache, it returns the evicted object. Otherwise, it returns NULL. 
```c
cache->evict(cache, req);
```

#### Cache remove
This is the lower level cache API, which tries to remove the object in the cache. If the object is in the cache and can be removed, it returns true. Otherwise, it returns false. 
Note that this function should not be used for eviction purpose.
```c
cache->remove(cache, obj_id);
```

#### Cache to evict
Sometimes you need to know what the cache will evict, but you don't want to evict it.
```c
cache->to_evict(cache, req);
```


### TraceReader APIs
There are mostly three APIs related to readers, `open_trace`, `close_trace`, `read_one_req`, let's take a look how
 they work.   

##### Setup a txt reader (trace can only contain request id)
```c
open_trace(data_path, PLAIN_TXT_TRACE, obj_id_type, NULL);
```
obj_id_type can be `OBJ_ID_NUM` or `OBJ_ID_STR`, if the object id is a number then use `OBJ_ID_NUM`, otherwise use
 `OBJ_ID_STR`. 
 
##### Setup a csv reader 
```c
reader_init_param_t init_params_csv = 
    {.delimiter=',', .time_field=2, .obj_id_field=6, .obj_size_field=4, .has_header=FALSE}; 
reader_t *reader_csv_c = open_trace("data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params_csv);
```

##### Setup a binary reader 
```c
reader_init_param_t init_params_bin = {.binary_fmt="<3I2H2Q", .obj_size_field=2, .obj_id_field=6, };
reader_t *reader_bin_l = setup_reader("data/trace.vscsi", BIN_TRACE, OBJ_ID_NUM, &init_params_bin);
```
The format of a binary trace is the same as 
[Python struct format specifier](https://docs.python.org/3/library/struct.html). 
 



### Simulator APIs 
The simulator API allows you to run multiple simulations at the same time. 
The different simulations can have different cache sizes or different cache eviction algorithms.

```c
// simulate multiple cache sizes specified using cache_sizes
// warmup_reader and warmup_perc is optional, if you do not need to warmup your cache, just pass `NULL` and 0. 
sim_res_t *
simulate_at_multi_sizes(reader_t reader*, 
                        cache_t *cache, 
                        int num_of_sizes, 
                        uint64_t *cache_sizes,
                        reader_t *warmup_reader, 
                        double warmup_perc, 
                        int num_of_threads);

// simulate multiple cache sizes from step_size to cache->cache_size
// it runs cache->cache_size/step_size simulations
sim_res_t *
simulate_at_multi_sizes_with_step_size(reader_t *reader, 
                                       cache_t *cache, 
                                       uint64_t step_size, 
                                       reader_t *warmup_reader, 
                                       double warmup_perc, 
                                       int num_of_threads);

// simulate with multiple caches, which can have different eviction algorithms or sizes
cache_stat_t *simulate_with_multi_caches(reader_t *reader, 
                                         cache_t *caches[],
                                         int num_of_caches,
                                         reader_t *warmup_reader,
                                         double warmup_frac, 
                                         int warmup_sec,
                                         int num_of_threads)
```

`simulate_at_multi_sizes` allows you to pass in an array of `cache_sizes` to simulate; 
`simulate_at_multi_sizes_with_step_size` allows you to specify the step size to simulate, the simulations will run at
cache sizes `step_size, step_size*2, step_size*3 .. cache->cache_size`. 
`simulate_with_multi_caches` allows you to pass in an array of `cache_t` to simulate, which can have different eviction algorithms or sizes.

The return result is an array of simulation results, the users are responsible for free the array. 
```c
typedef struct {
  uint64_t req_cnt;
  uint64_t req_bytes;
  uint64_t miss_cnt;
  uint64_t miss_bytes;
  uint64_t cache_size;
  cache_stat_t cache_state;
  void *other_data;   /* not used */
} sim_res_t;
```


### Trace utils 
#### get reuse/stack distance 
```c
// get the stack distance (number of uniq objects) since last access or till next request
// reader: trace reader
// dist_type: STACK_DIST or FUTURE_STACK_DIST
// return an array of int32_t with size written in array_size
int32_t *get_stack_dist(reader_t *reader, 
                        const dist_type_e dist_type,
                        int64_t *array_size);

// get the distance (number of requests) since last/first access
// reader: trace reader
// dist_type DIST_SINCE_LAST_ACCESS or DIST_SINCE_FIRST_ACCESS
// return an array of int32_t with size written in array_size
int32_t *get_access_dist(reader_t *reader, 
                         const dist_type_e dist_type,
                         int64_t *array_size);
```

## Examples 
#### C example


#### C++ example


### Build a cache hierarchy with multiple layers


### Build a cache cluster with consistent hashing


## FAQ 
#### Linking with libCacheSim
linking can be done in cmake or use pkg-config  
Such as in the `_build` directory:  
```
export PKG_CONFIG_PATH=$PWD
```

#### Possible problems
* if you get `error while loading shared libraries`, run `sudo ldconfig`



---





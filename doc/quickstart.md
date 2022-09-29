## libCacheSim Quickstart

### libCacheSim Modules 
Currently there are four main modules in libCacheSim, 
* **bin**: a set of binary tools including cachesim, traceConv, etc.
* **traceReader**: a module providing trace parsing and reading, currently supports csv, txt, and binary traces. 
* **cache**: it includes three modules --- eviction, admission and prefetching. 
  * **eviction**: provides a set of cache eviction algorithms such as LRU, LFU, FIFO, CLOCK, ARC, LFUDA (LFU with dynamic aging), SLRU, Hyperbolic, LHD, LeCaR, Cacheus, GLCache, etc. 
  * **admission**: provides a set of admission algorithms including size, bloomFilter, adaptSize.
  * **prefetch**: provides various prefetch algorithms, currently it is not used. 

---

use cases:
 cache hierarchy
 

### Common APIs 
#### traceReader 
There are mostly three APIs related to readers, *open_trace*, *close_trace*, *read_one_req*, let's take a look how
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
 

#### Cache
We can create a cache in two ways, first using plugin system, 
```c
cache_t *cache = create_cache(name, common_cache_params, cache_specific_params); 
```

The second approach is to explicitly call the cache initialization function, such as for LRU 
```c
cache_t *cache = LRU_init(common_cache_params, NULL); 
```
The common_cache_params is a common_cache_params_t struct, which contains the shared cache parameters 
```c
typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int hashpower;
} common_cache_params_t;
```
hashpower (`1 << hashpower`) is the estimated size of hash table (the number of objects stored in the cache
), default is 20, which means default hash table size is `1 << 20` (1048576) objects. Note that setting an appropriate 
hashpower can reduce the number of times hash table expands, but only set it if you know what you doing, otherwise
, leave it empty. 

#### Profiler 
There are two components under profiler, one is the simulator for calculating miss ratio curve by running multi-thread
 simulations, the other uses reuse distance to compute the miss ratio curve for LRU. 

*Simulator*
```c
sim_res_t *
simulate_at_multi_sizes(reader_t reader*, cache_t *cache, int num_of_sizes, uint64_t *cache_sizes,
                     reader_t *warmup_reader, double warmup_perc, int num_of_threads);

sim_res_t *
simulate_at_multi_sizes_with_step_size(reader_t *reader, cache_t *cache, uint64_t step_size, 
                     reader_t *warmup_reader, double warmup_perc, int num_of_threads);
```
warmup_reader and warmup_perc is optional, if you do not need to warmup your cache, just pass `NULL` and 0
. 
`simulate_at_multi_sizes` allows you to pass in an array of `cache_sizes` to simulate; 
`simulate_at_multi_sizes_with_step_size` allows you to specify the step size to simulate, the simulations will run at
cache sizes `step_size, step_size*2, step_size*3 .. cache->cache_size`. 

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

*LRU miss ratio profiler*
By computing stack distance (reuse distance in some text), libCacheSim is able to get the miss ratio at all
 possible cache sizes with *O(NlogN)* time complexity without running simulations.  

```c
double *get_lru_obj_miss_ratio(reader_t *reader, uint64_t max_size);
double *get_lru_obj_miss_ratio_curve(reader_t *reader, uint64_t max_size);
```



#### TraceStat 




## libCacheSim Quickstart

### libCacheSim Modules 
Currently there are four main modules in libCacheSim, 
* **traceReader**, which provides trace parsing and reading, currently supports csv, txt, and binary trace. 
* **traceStat**, which provides statistics about trace such as popularity, size distribution. 
* **cache**, which provides a set of cache eviction algorithms such as LRU, LFU, FIFO, MRU, SLRU, LHD. 
* **profiler**, which provides fast LRU miss ratio computation, reuse distance computation, a pre-built simulator.   
---

### Common APIs 
#### traceReader 
There are mostly three APIs related to readers, *open_trace*, *close_trace*, *read_one_req*, let's take a look how
 they work.   

##### Setup a txt reader (trace can only contain request id)
```
open_trace(data_path, PLAIN_TXT_TRACE, obj_id_type, NULL);
```
obj_id_type can be `OBJ_ID_NUM` or `OBJ_ID_STR`, if the object id is a number then use `OBJ_ID_NUM`, otherwise use
 `OBJ_ID_STR`. 
 
##### Setup a csv reader 
``` 
  reader_init_param_t *init_params_csv = (reader_init_param_t *)malloc(sizeof(reader_init_param_t));
  init_params_csv->delimiter = ',';
  init_params_csv->real_time_field = 2;     /* field index starts from 1 */
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = TRUE;
  reader_t *reader_csv_c = open_trace("data/trace.csv", CSV_TRACE, OBJ_ID_STR, init_params_csv);
  free(init_params_csv);
```

##### Setup a binary reader 
```
  reader_init_param_t *init_params_bin = (reader_init_param_t *)malloc(sizeof(reader_init_param_t));
  strcpy(init_params_bin->binary_fmt, "<3I2H2Q"); 
  init_params_bin->obj_size_field = 2;
  init_params_bin->obj_id_field = 6;
  init_params_bin->real_time_field = 7;
  reader_t *reader_bin_l = setup_reader("data/trace.vscsi", BIN_TRACE, OBJ_ID_NUM, init_params_bin);
  free(init_params_bin);
  return reader_bin_l;
```
format of a binary trace is the same as [Python struct format specifier](https://docs.python.org/3/library
/struct.html) 

#### Cache
```
    cache_t *cache = create_cache(name, common_cache_params, cache_specific_params)
    cache_t *cache = LRU_init(common_cache_params, NULL); 
```


#### Profiler 
```

double *get_lru_obj_miss_ratio(reader_t *reader, gint64 size);
double *get_lru_obj_miss_ratio_curve(reader_t *reader, gint64 size);

sim_res_t *
get_miss_ratio_curve(reader_t *const reader, const cache_t *const cache,
                     const gint num_of_sizes, const guint64 *const cache_sizes,
                     reader_t *const warmup_reader, const double warmup_perc,
                     const gint num_of_threads);
                     
                     
                     
sim_res_t *get_miss_ratio_curve_with_step_size(reader_t *const reader_in,
                                               const cache_t *const cache_in,
                                               const guint64 step_size,
                                               reader_t *const warmup_reader,
                                               const double warmup_perc,
                                               const gint num_of_threads);
                                               
                                               
                                               
typedef struct {
  uint64_t req_cnt;
  uint64_t req_bytes;
  uint64_t miss_cnt;
  uint64_t miss_bytes;
  uint64_t cache_size;
  cache_state_t cache_state;
  void *other_data;
} sim_res_t;


```





                                                        
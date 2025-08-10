# Profiling Memory Usage in CacheSim with Massif

## 1. Introduction
This guide explains how to analyze memory usage in the `cachesim` project using Valgrind's **massif** tool and identifies critical memory overhead areas as potential optimization targets. The workflow consists of the following steps:

- Heap memory profiling
- Identifying memory hotspots
- Optimization opportunities

## 2. Profiling Workflow

### 2.1 Heap memory profiling

1. Compile with debug symbols:
```sh
# Compile with debug symbols
mkdir _build && cd _build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
ninja
```

2. Run `massif` for a specific algorithm (e.g., S3-FIFO):

```sh
valgrind --tool=massif --massif-out-file=massif.out \
./bin/cachesim /path/to/wiki_2019t.oracleGeneral oracleGeneral s3fifo 1gb
```

3. Generate a human-readable report:
```sh
ms_print ./massif.out > massif.result
```

### 2.2 Analysis report

The generated report primarily includes a bar chart of memory usage (with instructions executed as the x-axis) and several heap profile snapshots. Some snapshots display detailed function call relationships showing how memory was allocated. Below is an example of such a report:


```sh
    MB
519.4^                                        :
     |#:::::::::::::@::::::::::::::::@::@@::::::::::::::::::::::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
   0 +----------------------------------------------------------------------->Gi
     0                                                                   652.3



--------------------------------------------------------------------------------
  n        time(i)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
  0              0                0                0             0            0
  1    174,738,746      542,473,864      541,412,480     1,061,384            0
99.80% (541,412,480B) (heap allocation functions) malloc/new/new[], --alloc-fns, etc.
->98.97% (536,870,912B) 0x483659: create_chained_hashtable_v2 (chainedHashTableV2.c:93)
| ->98.97% (536,870,912B) 0x48CCD6: cache_struct_init (cache.c:60)
|   ->74.23% (402,653,184B) 0x40FEB3: FIFO_init (FIFO.c:51)
|   | ->24.74% (134,217,728B) 0x423884: S3FIFO_init (S3FIFO.c:113)
|   | | ->24.74% (134,217,728B) 0x40880A: create_cache (cache_init.h:132)
|   | |   ->24.74% (134,217,728B) 0x40970B: parse_cmd (cli_parser.c:343)
|   | |     ->24.74% (134,217,728B) 0x406FFD: main (main.c:18)
|   | |
|   | ->24.74% (134,217,728B) 0x4238C0: S3FIFO_init (S3FIFO.c:118)
|   | | ->24.74% (134,217,728B) 0x40880A: create_cache (cache_init.h:132)
|   | |   ->24.74% (134,217,728B) 0x40970B: parse_cmd (cli_parser.c:343)
|   | |     ->24.74% (134,217,728B) 0x406FFD: main (main.c:18)
|   | |
|   | ->24.74% (134,217,728B) 0x423921: S3FIFO_init (S3FIFO.c:125)
|   |   ->24.74% (134,217,728B) 0x40880A: create_cache (cache_init.h:132)
|   |     ->24.74% (134,217,728B) 0x40970B: parse_cmd (cli_parser.c:343)
|   |       ->24.74% (134,217,728B) 0x406FFD: main (main.c:18)
|   |
|   ->24.74% (134,217,728B) 0x4236BB: S3FIFO_init (S3FIFO.c:81)
|     ->24.74% (134,217,728B) 0x40880A: create_cache (cache_init.h:132)
|       ->24.74% (134,217,728B) 0x40970B: parse_cmd (cli_parser.c:343)
|         ->24.74% (134,217,728B) 0x406FFD: main (main.c:18)
|
->00.84% (4,541,568B) in 1+ places, all below ms_print's threshold (01.00%)
```

The above report indicates that the memory usage of S3-FIFO remains nearly constant throughout instruction execution. The peak memory usage is `519.4MB`. `98.97%` of memory is used for `create_chained_hashtable_v2 (chainedHashTableV2.c:93)`. In the function `S3FIFO_init()`, there is one `cache_struct_init()` call and three `FIFO_init()` calls, each of which incurs another `cache_struct_init()`. In total, there are four `cache_struct_init()` calls, which create four hash tables. Each hash table occupies `24.74%` of the allocated memory.

**Optimization opportunities:** S3-FIFO can be implemented with a single hash table, which can reduce the memory usage.


## 3. Some Profiling Results and Observations


### 3.1. Memory Usage of Various Algorithms

We conducted tests using the `wiki_2019t` workload (available at [CMU's dataset repository](https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/wiki/)) to evaluate the memory usage of various algorithms with a cache size of 1GB.


| Algorithm   | Peak Memory Usage (MB) | Observations                                                                 |
| :---------- | :---------------- | :--------------------------------------------------------------------------- |
| FIFO        | 132.1             | Memory usage remains constant. Major memory consumption from `create_chained_hashtable_v2()`. |
| LRU         | 132.2             | Memory usage remains constant. Major memory consumption from `create_chained_hashtable_v2()`. |
| Clock       | 132.0             | Memory usage remains constant. Major memory consumption from `create_chained_hashtable_v2()`. |
| SLRU        | 132.6             | Memory usage remains constant. Major memory consumption from `create_chained_hashtable_v2()`. |
| LFU         | 131.4             | Memory usage remains constant. Major memory consumption from `create_chained_hashtable_v2()`. |
| ARC         | 813.1             | Memory usage increases steadily, peaking at the end. Major consumption from `create_chained_hashtable_v2()` and `create_cache_obj_from_request()`. |
| TwoQ        | 517.8             | Memory usage remains constant. Major consumption from `create_chained_hashtable_v2()`. Involves four calls (two FIFO, one for LRU, one for TwoQ itself). |
| GDSF        | 141.8             | Memory usage remains constant. Major consumption from `create_chained_hashtable_v2()`. |
| Hyperbolic  | 4.480             | Extremely low memory usage.  |
| LeCaR       | 192.3             | Memory usage increases steadily, peaking at the end. Major consumption from `create_chained_hashtable_v2()` and `insert_obj_info_freq_node()`. |
| Cacheus     | 805.2             | Memory usage remains constant. Major consumption from `create_chained_hashtable_v2()`. Involves multiple cache constructions like SR_LRU, CRLFU, and ghost cache. |
| LHD         | 206.0             | Memory usage remains constant. Major consumption from `create_chained_hashtable_v2()` and vector maintenance for HitDensity in LHD. |
| WTinyLFU    | 8580              | Memory usage remains constant. Major consumption from `minimalIncrementCBF_init()`, constructing a Counting Bloom Filter. |
| S3-FIFO     | 519.8             | Memory usage remains constant. Major consumption from `create_chained_hashtable_v2()`. Involves multiple cache constructions like Small FIFO, Main FIFO, Ghost FIFO. |
| Sieve       | 131.7             | Memory usage remains constant. Major consumption from `minimalIncrementCBF_init()`. |

### 3.2. Observations

1. **FIFO**, **LRU**, **Clock**, **SLRU**, **LFU**, **GDSF**, **Sieve** all use only one hashtable, thus their memory usage is low.
2. **TwoQ**, **Cacheus**, and **S3-FIFO** use multiple cache instances, resulting in multiple hashtables internally, which leads to higher memory usage.
3. **Hyperbolic** uses a smaller hashpower during initialization, thus its memory usage is low.
4. **LeCaR** and **LHD** use hashtables while also maintaining additional metadata, which increases memory usage.
5. **WTinyLFU** uses a significant amount of memory to store Bloom Filters, thus its memory usage is high.
6. **ARC** has high memory usage, the reason for which is unclear.

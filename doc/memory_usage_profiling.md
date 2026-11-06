# Profiling memory usage

This page shows how to find out where `cachesim` spends its memory, using Valgrind's **massif** heap profiler. Use it when a simulation needs more RAM than you expected, or when you want to make an algorithm leaner.

The quick answer for most runs: memory goes almost entirely to the object hash table. Algorithms built from several internal caches have several hash tables, and they use several times more memory.

## Profile a run

**1. Build a debug binary.** Debug builds keep the symbols massif needs to name functions. They also leave out tcmalloc, which doesn't work well under Valgrind. The debug script builds one in `_build_dbg/`:

```bash
./scripts/debug.sh
```

**2. Run `cachesim` under massif.** For example, S3-FIFO with a 1 GB cache:

```bash
valgrind --tool=massif --massif-out-file=massif.out \
    ./_build_dbg/bin/cachesim /path/to/wiki_2019t.oracleGeneral oracleGeneral s3fifo 1gb
```

Valgrind slows the program down a lot. Try a smaller trace or `--num-req` first.

**3. Turn the output into a readable report.**

```bash
ms_print massif.out > massif.result
```

## Read the report

The report starts with a chart of heap size over time, where time is measured in executed instructions. Below the chart are snapshots, and some of them show the call tree that made each allocation. Here is a shortened report for S3-FIFO:

```text
    MB
519.4^                                        :
     |#:::::::::::::@::::::::::::::::@::@@::::::::::::::::::::::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
     |#::: :: :: :::@: ::::: : ::: ::@::@ ::: :: : : ::::::: :::::@::@::@::@::
   0 +----------------------------------------------------------------------->Gi
     0                                                                   652.3

--------------------------------------------------------------------------------
  n        time(i)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
  1    174,738,746      542,473,864      541,412,480     1,061,384            0
99.80% (541,412,480B) (heap allocation functions) malloc/new/new[], --alloc-fns, etc.
->98.97% (536,870,912B) 0x483659: create_chained_hashtable_v2 (chainedHashTableV2.c:93)
| ->98.97% (536,870,912B) 0x48CCD6: cache_struct_init (cache.c:60)
|   ->74.23% (402,653,184B) 0x40FEB3: FIFO_init (FIFO.c:51)
|   | ->24.74% (134,217,728B) 0x423884: S3FIFO_init (S3FIFO.c:113)
|   | ->24.74% (134,217,728B) 0x4238C0: S3FIFO_init (S3FIFO.c:118)
|   | ->24.74% (134,217,728B) 0x423921: S3FIFO_init (S3FIFO.c:125)
|   |
|   ->24.74% (134,217,728B) 0x4236BB: S3FIFO_init (S3FIFO.c:81)
|
->00.84% (4,541,568B) in 1+ places, all below ms_print's threshold (01.00%)
```

Here's how to read it:

* The flat chart means memory stays almost constant for the whole run. It peaks at 519.4 MB.
* 98.97% of the heap comes from `create_chained_hashtable_v2`, the hash table that maps object ids to cached objects.
* `S3FIFO_init` calls `cache_struct_init` once for itself, and it calls `FIFO_init` three times for its small, main and ghost queues. Each `FIFO_init` calls `cache_struct_init` again. That makes four hash tables of about 128 MiB each, roughly a quarter of the heap apiece.

That points straight at an optimization: S3-FIFO could share one hash table across its queues and use about a quarter of the memory.

## Shrinking the hash table

`cachesim` sizes each hash table to 2^`hashpower` entries. The default is 24, or about 16M entries, no matter how small the trace is. On a small trace, lowering it cuts memory a lot. Replaying the sample trace with LRU drops from about 106 MB to about 8 MB at `--hashpower=16`:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --hashpower=16
```

The table still grows when it fills up, so a small value is safe. It just costs some resizing on large traces. Sampling-based algorithms draw eviction candidates from the hash table, so their miss ratios shift slightly with `--hashpower`. Keep it fixed when you compare results.

## Measured memory usage by algorithm

These numbers come from the `wiki_2019t` workload (available from [CMU's dataset repository](https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/wiki/)) with a 1 GB cache.

| Algorithm | Peak memory (MB) | Where the memory goes |
| :--- | ---: | :--- |
| FIFO | 132.1 | Constant. Mostly one hash table (`create_chained_hashtable_v2()`). |
| LRU | 132.2 | Constant. Mostly one hash table. |
| Clock | 132.0 | Constant. Mostly one hash table. |
| SLRU | 132.6 | Constant. Mostly one hash table. |
| LFU | 131.4 | Constant. Mostly one hash table. |
| GDSF | 141.8 | Constant. Mostly one hash table. |
| Sieve | 131.7 | Constant. Mostly one hash table. |
| Hyperbolic | 4.48 | Very low, because it starts with a small hashpower. |
| LeCaR | 192.3 | Grows steadily. Hash table plus frequency metadata (`insert_obj_info_freq_node()`). |
| LHD | 206.0 | Constant. Hash table plus the vectors LHD keeps for hit density. |
| TwoQ | 517.8 | Constant. Four hash tables: two FIFOs, one LRU and TwoQ itself. |
| S3-FIFO | 519.8 | Constant. Four hash tables: small, main and ghost FIFOs plus S3-FIFO itself. |
| Cacheus | 805.2 | Constant. Several internal caches (SR-LRU, CR-LFU, ghost caches), each with its own hash table. |
| ARC | 813.1 | Grows steadily, peaking at the end. Hash tables plus per-object allocations (`create_cache_obj_from_request()`). |
| WTinyLFU | 8580 | Constant. Dominated by its counting Bloom filter (`minimalIncrementCBF_init()`). |

## What the numbers tell you

1. **One hash table means low memory.** FIFO, LRU, Clock, SLRU, LFU, GDSF and Sieve each use a single hash table, and they all sit around 130 MB.
2. **Composite algorithms pay per sub-cache.** TwoQ, S3-FIFO and Cacheus are built from several internal caches, and each one has its own hash table. That is where their roughly 4x to 6x overhead comes from, and it's the most promising place to save memory.
3. **Hyperbolic** is tiny because it starts with a smaller hashpower.
4. **LeCaR and LHD** add their own metadata on top of the hash table.
5. **WTinyLFU** spends most of its memory on its counting Bloom filter.
6. **ARC's** high usage is not yet explained. If you look into it, we'd love to hear what you find.

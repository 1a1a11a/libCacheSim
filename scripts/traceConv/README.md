
## What is this about
This folder contains the scripts that convert the orignal traces and produce a [lcs](https://github.com/1a1a11a/libCacheSim/blob/develop/libCacheSim/traceReader/customizedReader/lcs.h) trace. 

The convert process has two steps:
1. It pre-processes the original trace and normalize it into a human readable csv file
2. It runs `traceConv` to convert the csv trace to lcs format


The pre-processing is the major part of scripts and perform the following functions
* converts from any format to csv files of standard format, i.e., "timestamp, id, size, op, tenant, ttl" where `op`, `tenant` and `ttl` are optional
* change timestamp to use second as the time unit
* for key-value traces
  * it expands the line to multiple requests if a trace has `op_cnt` field
  * it backfills cache miss object size from a later set request because cache miss has size zero, if the object is not SET in the trace, we filter out size zero requests
  * it backfills ttl from a later set request (we use the last object size to avoid size change problem)
  * it provides a sampling function to provide sampled lcs traces
* for block traces
  * it splits a large request for multiple blocks to 4K blocks
  * it uses logical block address (LBA) as the object id and aligns the LBA to 4K blocks, and it uses bytes as the request size. Note that some traces use logical block number (LBN) as id, we convert it to LBA by multiplying BLOCK_SIZE and some use the nubmer of sectors as request size
  * it maps the same LBA from different volumes to different LBAs by adding vol_id * 100 TiB


To print the trace, you can use `bin/tracePrint` from libCacheSim or `scripts/lcs_reader.py` 




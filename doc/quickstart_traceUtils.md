
## Other trace utilities
We also provide some trace utilities to help you use the traces and debug applications. 

### tracePrint
Print requests from a trace.

```bash
# print 10 requests from a trace
./tracePrint ../data/trace.vscsi vscsi -n 10
```

### traceConv
Convert a trace to libCacheSim format so it has a smaller size, contains next request time (oracle information) and runs faster. 
```bash
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin
```
Note that the conversion supports all trace types including csv trace. Note that if the object id is numeric, add the option -e `obj_id_is_num=1` to the command line, which reduces memory usage.


We can also sample a trace to reduce the size of the trace. 
```bash
# sample 1% of the trace
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin -s 0.01
```


### traceFilter
traceFilter simulates a multi-layer cache hierarchy. It filters the trace based on the cache hit/miss information and generates a trace for the second layer. 
The generated trace is in oracleGeneral format. 

```bash 
# filter trace using a cache with a size 0.01 of the working set size and the FIFO eviction policy
./bin/traceFilter ../data/trace.vscsi vscsi --filter-type fifo --filter-size 0.01 --ignore-obj-size 1
```



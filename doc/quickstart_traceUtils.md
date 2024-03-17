
## Other trace utilities
We also provide some trace utilities to help you use the traces and debug applications. 

### tracePrint
Print requests from a trace.

```bash
# print 10 requests from a trace
./bin/tracePrint ../data/cloudPhysicsIO.vscsi vscsi -n 10
```

### traceConv
Convert a trace to oracleGeneral format so you can run it faster (10x speedup) using less memory. Meanwhile, the generated trace has a smaller size, contains next request time. 
```bash
# the first parameter is the input trace, the second parameter is trace type, the third parameter is the output trace path
./bin/traceConv ../data/cloudPhysicsIO.txt txt cloudPhysicsIO.oracleGeneral.bin
```
Note that the conversion supports all trace types including csv trace. Moreover, if the object id is numeric, add the option `-e obj_id_is_num=1` to the command line, which reduces memory usage.


We can also sample a trace to reduce the size of the trace. 
```bash
# sample 1% of the trace
./bin/traceConv ../data/cloudPhysicsIO.vscsi vscsi ../data/cloudPhysicsIO.oracleGeneral -s 0.01
```


### traceFilter
traceFilter simulates a multi-layer cache hierarchy. It filters the trace based on the cache hit/miss information and generates a trace for the second layer. 
The generated trace is in oracleGeneral format. 

```bash 
# filter trace using a cache with a size 0.01 of the working set size and the FIFO eviction policy
./bin/traceFilter ../data/trace.vscsi vscsi --filter-type fifo --filter-size 0.01 --ignore-obj-size 1
```



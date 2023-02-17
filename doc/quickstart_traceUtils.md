
## Other trace utilities
We also provide some trace utilities to help you use the traces and debug applications. 

### tracePrint
Print requests from a trace.

```bash
# print 10 requests from a trace
./tracePrint ../data/trace.vscsi vscsi -n 10
```

### traceStat
ongoing

### traceGen
ongoing


### traceConv
Convert a trace to libCacheSim format so it has a smaller size and runs faster. 
```bash
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin
```

We can also sample a trace to reduce the size of the trace. 
```bash
# sample 1% of the trace
./traceConv ../data/trace.vscsi vscsi ../data/trace.vscsi.bin -s 0.01
```


### traceFilter





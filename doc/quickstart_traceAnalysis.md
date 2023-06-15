
## Trace analysis tool
libCacheSim provides a set of tools to help you analyze traces. 
After building the project, you can find a binary called `traceAnalysis`.
This doc shows how to use the tool.
If you are interested, the source code is located in the `bin/traceAnalysis/` and `traceAnalysis` directory.
We also provide some trace utilities to help you use the traces and debug applications. 

### Obtain tace statistics
```bash
./traceAnalysis PATH_TO_TRACE traceType 
```

Print requests from a trace.

```bash
# print 10 requests from a trace
./tracePrint ../data/trace.vscsi vscsi -n 10
```






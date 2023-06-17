
## Trace analysis tool
libCacheSim provides a set of tools to help you analyze traces. 
After building the project, you can find a binary called `traceAnalyzer`.
This doc shows how to use the tool.
If you are interested, the source code is located in the `bin/traceAnalyzer/` and `traceAnalyzer` directory.

### Obtain trace statistics
Usage `traceAnalyzer PATH_TO_TRACE traceType [--task1] [--task2]`.

Example
```bash
# run all common tasks
./bin/traceAnalyzer PATH_TO_TRACE traceType --common
```

### Plot trace statistics and visualize the trace
We provide plot scripts in `scripts/traceAnalysis/` to help you plot the trace statistics.
After generating plot data, we can plot access pattern, request rate, size, reuse, and popularity using the following commands:

```bash
python3 traceAnalysis/access_pattern.py ${dataname}.access
python3 traceAnalysis/req_rate.py ${dataname}.reqRate_w300
python3 traceAnalysis/size.py ${dataname}.size
python3 traceAnalysis/reuse.py ${dataname}.reuse
python3 traceAnalysis/popularity.py ${dataname}.popularity

# plot more expensive analysis
python3 traceAnalysis/size_heatmap.py ${dataname}.sizeWindow_w300
python3 traceAnalysis/popularity_decay.py ${dataname}.popularityDecay_w300
python3 traceAnalysis/reuse_heatmap.py ${dataname}.reuseWindow_w300
```


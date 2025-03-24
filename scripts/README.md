# Installation, plotting miss ratio curves, and trace analysis

##  Installation 
```bash
# install dependency
bash install_dependency.sh

# install libCacheSim
bash install_libCacheSim.sh
```

## Plot miss ratio curves
```bash
# plot miss ratio over sizes 
python3 plot_mrc_size.py \
--tracepath ../data/twitter_cluster52.csv --trace-format csv \
--trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,,obj-id-is-num=1" \
--algos=fifo,lru,lecar,s3fifo

# plot miss ratio over time
python3 plot_mrc_time.py \
--tracepath ../data/twitter_cluster52.csv --trace-format csv \
--trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,,obj-id-is-num=1" \
--algos=fifo,lru,lecar,s3fifo \
--report-interval 120
```

## Profile miss ratio curves

`mrcProfiler` enables fast profiling of miss ratio curves. With a proper parameter setting (such as `sampling_rate`, see [here](../doc/quickstart_mrcProfiler.md) for details) `mrcProfiler` can profile miss ratio curves in a fast way with acceptable accuracy.

```bash
# plot profiled miss ratio over sizes
python3 profile_mrc.py \
--tracepath ../data/twitter_cluster52.csv --trace-format csv \
--trace-format-params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,,obj-id-is-num=1" \
--profiler=MINISIM --algos=LRU,FIFO,ARC,S3FIFO --profiler-params=FIX_RATE,0.1,10 \
--sizes=0.01,0.02,0.05,0.10,0.20,0.40
```

## Trace analysis
### Generate the plot data
Plot data are generated using `traceAnalyzer` using 
```
./bin/traceAnalyzer /path/trace trace_format --common
```

### Visualize the trace
Then we can plot access pattern, request rate, size, reuse, and popularity using the following commands:

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

## Note
- The support for the Belady and BeladySize algorithms is limited to oracleGeneral traces because these traces contain future request information that Belady and BeladySize rely on.
- When the object size is considered (i.e., `--ignore-obj-size 1` is **not** provided as a command-line argument), BeladySize should be used instead of Belady.

## Generate Synthetic Workload

We can generate a Zipf-distributed workload in either txt or oracleGeneral format using [data_gen.py](./data_gen.py). The skewness of the distribution can be controlled with the `--alpha` parameter (greater than or equal to 0). A smaller value of `--alpha` results in less skewness. When `--alpha` is set to 0, the distribution reduces to a uniform distribution.

```bash
python3 data_gen.py -m 1000000 -n 100000000 --alpha 1 > /disk/data/zipf_1_1_100.txt

python3 data_gen.py -m 10000000 -n 100000000 --alpha 1 --bin-output zipf_1_10_100.oracleGeneral
```

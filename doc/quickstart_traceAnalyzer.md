
## Trace analysis tool
libCacheSim provides a set of tools to help you analyze traces. After building the project, you can find a binary called `traceAnalyzer`.
This doc shows how to use the tool.
If you are interested, the source code is located in the [bin/traceAnalyzer/](/libCacheSim/bin/traceAnalyzer) and [traceAnalyzer](/libCacheSim/traceAnalyzer) directory.

### Obtain trace statistics
#### Usage: 
```
# ./bin/traceAnalyzer --help for a list of tasks and options
./bin/traceAnalyzer PATH_TO_TRACE traceType [--task1] [--task2]
```

#### A list of tasks:
* `--common`: run all common tasks, including `--stat`, `--traceStat`, `--reqRate`, `--size`, `--reuse`, `--popularity`
* `--all`: run all tasks
* `--accessPattern`: generate access pattern data for plotting using [scripts/traceAnalysis/access_pattern.py](/scripts/traceAnalysis/access_pattern.py)
* `--reqRate`: generate request rate data for plotting using [scripts/traceAnalysis/req_rate.py](/scripts/traceAnalysis/req_rate.py)
* `--size`: generate size distribution data for plotting using [scripts/traceAnalysis/size.py](/scripts/traceAnalysis/size.py) and [scripts/traceAnalysis/size_heatmap.py](/scripts/traceAnalysis/size_heatmap.py)
* `--reuse`: generate reuse distribution data for plotting using [scripts/traceAnalysis/reuse.py](/scripts/traceAnalysis/reuse.py) and [scripts/traceAnalysis/reuse_heatmap.py](/scripts/traceAnalysis/reuse_heatmap.py)
* `--popularity`: generate popularity data for plotting using [scripts/traceAnalysis/popularity.py](/scripts/traceAnalysis/popularity.py)
* `--popularityDecay`: generate popularity data for plotting using [scripts/traceAnalysis/popularity_decay.py](/scripts/traceAnalysis/popularity_decay.py)

#### Example: 
```bash
# run all common tasks
./bin/traceAnalyzer PATH_TO_TRACE traceType --common
```

The trace analyzer will generate statistics of the trace and save them to `stat` and `traceStat` files.


<details>
  <summary style="background-color: #f2f2f2; padding: 10px; border: 1px solid #ccc; border-radius: 4px;">An example output running a block cache workload:</summary>

    dat: w92.oracleGeneral.bin.zst
    number of requests: 4284658, number of objects: 606386
    number of req GiB: 133.5165, number of obj GiB: 21.7219
    compulsory miss ratio (req/byte): 0.1415/0.1627
    object size weighted by req/obj: 33459/38463
    frequency mean: 7.0659
    time span: 609774(7.0576 day)
    request rate min 0.5400 req/s, max 257.7533 req/s, window 300s
    object rate min 0.1333 obj/s, max 256.6933 obj/s, window 300s
    X-hit (number of obj accessed X times): 5720(0.0094), 4813(0.0079), 4514(0.0074), 5044(0.0083), 34919(0.0576), 529106(0.8726), 2551(0.0042), 2161(0.0036), 
    freq (fraction) of the most popular obj: 74030(0.0173), 74007(0.0173), 59484(0.0139), 39656(0.0093), 27403(0.0064), 27391(0.0064), 21380(0.0050), 19828(0.0046), 

</details>


<details>
  <summary style="background-color: #f2f2f2; padding: 10px; border: 1px solid #ccc; border-radius: 4px;">A second example showing the first 10 million requests of the Twitter cluster52 trace (can be found in the data directory)
</summary>

    dat: ../data/twitter_cluster52_10m.csv
    number of requests: 10000000, number of objects: 897664
    number of req GiB: 1.8806, number of obj GiB: 0.1627
    compulsory miss ratio (req/byte): 0.0898/0.0865
    object size weighted by req/obj: 201/194
    frequency mean: 11.1400
    time span: 5293(0.0613 day)
    write: 0(0), overwrite: 0(0), del:0(0)
    request rate min 1753.7533 req/s, max 1986.3433 req/s, window 300s
    object rate min 300.3567 obj/s, max 319.8633 obj/s, window 300s
    popularity: Zipf linear fitting slope=0.9472
    X-hit (number of obj accessed X times): 323699(0.3606), 218436(0.2433), 51516(0.0574), 128181(0.1428), 48785(0.0543), 25172(0.0280), 14606(0.0163), 14769(0.0165), 
    freq (fraction) of the most popular obj: 546563(0.0547), 365140(0.0365), 221311(0.0221), 190811(0.0191), 154037(0.0154), 151832(0.0152), 127070(0.0127), 98851(0.0099), 
</details>

----

### Plot trace statistics and visualize the trace
We provide plot scripts in [scripts/traceAnalysis/](/scripts/traceAnalysis/) to help you plot the trace statistics.
After generating plot data, we can plot access pattern, request rate, size, reuse, and popularity using the following commands:

#### Access pattern
```bash
# plot the access pattern using wall clock (real) time
python3 scripts/traceAnalysis/access_pattern.py ${dataname}.accessRtime

# plot the access pattern using logical/virtual (request count) time 
python3 scripts/traceAnalysis/access_pattern.py ${dataname}.accessVtime
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_access_rt.svg" alt="access pattern w92_r" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/w92_access_vt.svg" alt="access pattern w92_v" width="40%">
</div>
<div style="text-align: center; color: grey;">
A block cache workload (w92). The left column shows wall clock time, this workload is mostly driven by a daily scan; the right column shows virtual (request count) time. 
</div>
<br>

<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/twitter_cluster52_10m_access_rt.svg" alt="access pattern twitter_r" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_access_vt.svg" alt="access pattern twitter_v" width="40%">
</div>
<div style="text-align: center; color: grey;">
The first 10m requests of the Twitter cluster52 trace, this is a Zipf workload. 
</div>
<br>



#### Request rate
```bash
# this is only supported for traces that have (wall clock) time field
python3 scripts/traceAnalysis/req_rate.py ${dataname}.reqRate_w300
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_reqRate.svg" alt="request rate w92" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_reqRate.svg" alt="request rate twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
Left: a block cache workload (w92), right: the first 10m requests of the Twitter cluster52 trace. <br>
The block workload has a daily request spike, while the Twitter workload is too short to observe a pattern. 
</div>
<br>


#### Size distribution
```bash
# this is only supported for traces that have object size
python3 scripts/traceAnalysis/size.py ${dataname}.size
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_size.svg" alt="size w92" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_size_log.svg" alt="size twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
Left: a block cache workload (w92), right: the first 10m requests of the Twitter cluster52 trace. <br>
The block workload has most objects being 4 KiB and 64 KiB, while the Twitter workload has most objects being 64 B. <br>
The Request curve is weighted by request count, and the Object curve is weighted by object count. 
</div>
<br>


#### Reuse distribution
This is the time since the last access of the object.

```bash
python3 scripts/traceAnalysis/reuse.py ${dataname}.reuse
```

Some example plots are shown below 
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_reuse_rt_log.svg" alt="reuse w92_r" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/w92_reuse_vt_log.svg" alt="reuse w92_v" width="40%">
</div>
<div style="text-align: center; color: grey;">
A block cache workload (w92). The left column shows wall clock time, the right column shows virtual (request count) time. Most reuses happen around one day.
</div>
<br>

<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/twitter_cluster52_10m_reuse_rt.svg" alt="reuse twitter_r" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_reuse_vt_log.svg" alt="reuse twitter_v" width="40%">
</div>

<div style="text-align: center; color: grey;">
The first 10m requests of the Twitter cluster52 trace. The left column shows wall clock time, the right column shows virtual (request count) time. Most requests happen soon after the object is accessed. 
</div>
<br>

#### Popularity
```bash
# the popularity skewness ($\alpha$) is in the output of traceAnalyzer
# this plots the request count/freq over object rank
# note that measuring popularity plot does not make sense for very small traces and some block workloads  
# and note that popularity is highly affected by the layer of the cache hierarchy
python3 scripts/traceAnalysis/popularity.py ${dataname}.popularity
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/twitter_cluster52_10m_pop_rank.svg" alt="popularity twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
The first 10m requests of the Twitter cluster52 trace. <br>
</div>
<br>





#### Size distribution heatmap
This and the following plots are more expensive plots that require more CPU cycles and DRAM usage to generate. 
This plot requires wall clock time and object size in the trace. 
This is a heatmap of the size distribution of the trace. The x-axis is the clock time, and the y-axis is the size. The color represents the number of requests having a certain size range at that time. The darker the color, the more requests of the certain size at that time. 
The heatmap is generated using the following command:

```bash
python3 scripts/traceAnalysis/size_heatmap.py ${dataname}.sizeWindow_w300
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_size_heatmap_req.svg" alt="popularity w92" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_size_heatmap_req.svg" alt="popularity twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
Left: a block cache workload (w92), right: the first 10m requests of the Twitter cluster52 trace. The two plots show the distribution weighted by request, there are also two figures weighted by object count. <br>
</div>
<br>


#### Reuse distribution heatmap
This is a heatmap of the reuse distribution of the trace. The x-axis is the wall clock time, and the y-axis is the reuse time (in seconds) or reuse distance (the number of requests since last access of the object). The color represents the number of requests having the reuse time or reuse distance. 
The heatmap is generated using the following command:

```bash
python3 scripts/traceAnalysis/reuse_heatmap.py ${dataname}.reuseWindow_w300
```

Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/w92_reuse_heatmap_vt.svg" alt="popularity w92" width="40%">
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="/doc/plot/twitter_cluster52_10m_reuse_heatmap_vt.svg" alt="popularity twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
Left: a block cache workload (w92), right: the first 10m requests of the Twitter cluster52 trace. <br>
</div>
<br>


#### popularity decay
There are two versions of the plots, one is line plot, and the other is a heatmap.

```bash
# this requires a long trace (e.g., 7 day) to generate a meaningful plot
# and most block workloads do not have enough requests to plot meaningful popularity decay
python3 scripts/traceAnalysis/popularity_decay.py ${dataname}.popularityDecay_w300_obj
```

<!-- Some example plots are shown below:
<div style="display: flex; justify-content: center; align-items: center;">
<img src="/doc/plot/twitter_cluster52_10m_popularityDecayLineLog.svg" alt="popularity twitter" width="40%">
</div>
<div style="text-align: center; color: grey;">
Left: a block cache workload (w92), right: the first 10m requests of the Twitter cluster52 trace. <br>
The block workload has most objects being 4 KiB and 64 KiB, while the Twitter workload has most objects around 64 B. <br>
The Request curve is weighted by request count, and the Object curve is weighted by object count. 
</div>
<br> -->

### Advanced features 
```bash
# cap the number of requests read from the trace
./traceAnalyzer --num-req=1000000 ../data/trace.vscsi vscsi

# change output 
./traceAnalyzer -o my-output ../data/trace.vscsi vscsi

# use part of the trace to warm up the cache
./traceAnalyzer --warmup-sec=86400 ../data/trace.vscsi vscsi
```

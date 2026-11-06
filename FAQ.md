# FAQ

The commands below are run from the build directory (`_build/`), so the sample traces are at `../data/`.

### How do I read an oracleGeneral trace, and how do I convert a csv trace into one?

[oracleGeneral](/libCacheSim/traceReader/customizedReader/oracle/oracleGeneralBin.h) is a binary format, so you can't open it in a text editor. Each request is stored as this struct:

```c
struct {
    uint32_t clock_time;
    uint64_t obj_id;
    uint32_t obj_size;
    int64_t next_access_vtime;  // -1 if there is no next access
};
```

* **To read it**, use `tracePrint`, which prints the trace as plain text. It is built into `bin/` next to `cachesim`.
  ```bash
  ./bin/tracePrint ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral
  ```
* **To convert a csv trace**, use `traceConv`. See [quickstart_traceUtils.md](/doc/quickstart_traceUtils.md), or run `./bin/traceConv --help`.
  ```bash
  ./bin/traceConv ../data/cloudPhysicsIO.csv csv \
      -t "time-col=2,obj-id-col=5,obj-size-col=4,obj-id-is-num=1" \
      --output-format=oracleGeneral
  ```

oracleGeneral traces are usually stored zstd-compressed. libCacheSim reads them without decompressing them first.

### What are the units in a trace?

In the sample [cloudPhysicsIO.csv](/data/cloudPhysicsIO.csv), time is in seconds and object size is in bytes.

`next_access_vtime` is a *logical* time. It holds the 1-based position of the request where this object is next asked for, or `-1` if it is never asked for again. It is an absolute position in the trace, not the distance to the next request. For example, in `cloudPhysicsIO.oracleGeneral.bin`, request 7 stores `19`, and that object is next requested at request 19. Algorithms subtract the current request count themselves, so storing a distance here would quietly change which objects they evict.

Object ids are hashed unless you tell the reader they are already numbers. If the id column holds numbers, pass `obj-id-is-num=true` in `--trace-type-params`. `cachesim` stops with an error if you leave it out on such a trace.

### Which algorithms need an oracle trace?

Belady, BeladySize and BeladyCompute (`belady`, `beladySize`, `beladyCompute`) need to know when each object will be requested next. Only the oracle formats `oracleGeneral` and `lcs` carry that, so these algorithms only run on those. On any other trace, `cachesim` stops with a message like:

```text
belady is only supported for oracleGeneral and lcs trace
```

Convert your trace with `traceConv` first (see the question above). `cachesim` currently checks for this using the file name, so keep `oracleGeneral` or `lcs` in the name of your converted trace.

If an oracle trace has zero-size requests that the reader skipped, its next-access times may point at requests that are no longer there. See [oracle_traces.md](/doc/oracle_traces.md) for how to repair it.

### Why does `cachesim` say "do not support algorithm X"?

Some algorithms pull in extra dependencies, so they are behind an optional build flag: GLCache (`-DENABLE_GLCACHE=ON`), LRB (`-DENABLE_LRB=ON`), 3LCache (`-DENABLE_3L_CACHE=ON`) and S4-FIFO (`-DENABLE_S4FIFO=ON`). Rebuild with the flag you need. The [README](/README.md#supported-algorithms) lists every algorithm and the flag it needs.

### Why does `--warmup-sec` make my run report 0 requests?

The warmup is longer than the trace. Requests during the warmup fill the cache but aren't counted, so if the warmup covers the whole trace, nothing is left to measure. The sample `cloudPhysicsIO` trace covers only two hours (7200 seconds), so this run reports `0 req`:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb --warmup-sec=86400
```

Pick a warmup shorter than the trace's time span. `traceAnalyzer` prints the time span (see [quickstart_traceAnalyzer.md](/doc/quickstart_traceAnalyzer.md)).

### Where can I get larger traces?

The traces in [data/](/data/) are samples. They are **far too small to compare miss ratios between algorithms**. We keep a list of open-source cache datasets at [cacheMon/cache_dataset](https://github.com/cacheMon/cache_dataset).

---

More questions? Check the [documentation index](/doc/README.md), search the [issue tracker](https://github.com/1a1a11a/libCacheSim/issues), or ask in [Discussions](https://github.com/1a1a11a/libCacheSim/discussions).

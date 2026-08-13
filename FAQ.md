# FAQ

### How do I read an oracleGeneral trace, and how do I convert a csv trace into one?

The [oracleGeneral](/libCacheSim/traceReader/customizedReader/oracle/oracleGeneralBin.h) trace is a binary format, so it cannot be read as a text file. Each request is the following struct:

```c
struct {
    uint32_t clock_time;
    uint64_t obj_id;
    uint32_t obj_size;
    int64_t next_access_vtime;  // -1 if there is no next access
};
```

* **Read the trace**: use `tracePrint` to print the trace as plain text. It is built into `bin/` alongside `cachesim`.
  ```bash
  ./bin/tracePrint ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral
  ```
* **Convert a csv trace**: use `traceConv`. See [quickstart_traceUtils.md](/doc/quickstart_traceUtils.md), or run `./bin/traceConv --help`.
  ```bash
  ./bin/traceConv ../data/cloudPhysicsIO.csv csv \
      -t "time-col=2,obj-id-col=5,obj-size-col=4,obj-id-is-num=1" \
      --output-format=oracleGeneral
  ```

oracleGeneral traces are usually stored zstd-compressed, and libCacheSim reads them without decompressing first.

### What are the units in a trace?

In the sample [cloudPhysicsIO.csv](/data/cloudPhysicsIO.csv), time is in seconds and object size is in bytes.

`next_access_vtime` is a *logical* time: the number of requests between the current request and the next request to the same object, or `-1` when the object is never accessed again. Algorithms that need future information, such as [Belady](/libCacheSim/cache/eviction/Belady.c) and BeladySize, rely on it, which is why they only work on oracle traces.

Object ids are hashed unless the reader is told they are already numeric. Pass `obj-id-is-num=true` in `--trace-type-params` when the id column holds numbers — `cachesim` stops with an error if you leave it out on such a trace.

### Why does `cachesim` say "do not support algorithm X"?

Some algorithms are behind an optional build flag because they pull in extra dependencies: GLCache (`-DENABLE_GLCACHE=ON`), LRB (`-DENABLE_LRB=ON`), and 3LCache (`-DENABLE_3L_CACHE=ON`). Rebuild with the relevant flag to enable them. See the [README](/README.md#supported-algorithms) for the full list.

### Where can I get larger traces?

The traces in [data/](/data/) are samples and are **far too small to compare miss ratios between algorithms**. We maintain a list of open-source cache datasets at [cacheMon/cache_dataset](https://github.com/cacheMon/cache_dataset).

---

More questions? Check the [documentation index](/doc/README.md), search the [issue tracker](https://github.com/1a1a11a/libCacheSim/issues), or ask in [Discussions](https://github.com/1a1a11a/libCacheSim/discussions).

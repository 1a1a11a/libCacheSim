# Trace utilities: tracePrint, traceConv, traceFilter

libCacheSim ships three small tools for working with trace files:

* **tracePrint** prints a trace as plain text, which is handy for binary traces you cannot open in an editor.
* **traceConv** converts a trace to a compact binary format that `cachesim` reads several times faster.
* **traceFilter** runs a trace through a small cache and keeps only the misses, giving you the trace the next cache layer would see.

All commands run from the build directory, `_build/`, so the sample traces are at `../data/`. The three tools share the reader options of `cachesim`: `-t` for csv column settings, `-s` for sampling, and `--ignore-obj-size`. Run any of them with `--help` to see every option.

## Print a trace with tracePrint

```bash
./bin/tracePrint ../data/cloudPhysicsIO.vscsi vscsi -n 5
```

```text
# time,object,size
5633898,42932745,512
5633898,42932746,512
5633898,42932747,512
5633898,40409911,6656
5633899,31954535,6144
```

`-n` limits how many requests are printed; leave it out to print the whole trace. For an oracle trace, a fourth column shows `next_access_vtime`, the position of the next request to the same object. Objects that are never requested again show `9223372036854775807` (the largest 64-bit integer):

```bash
./bin/tracePrint ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral -n 3
```

```text
# time,object,size,next_access_vtime
5633898,42932745,512,9223372036854775807
5633898,42932746,512,9223372036854775807
5633898,42932747,512,9223372036854775807
```

Other options: `--obj-id-only=1` prints just the object ids, and `--field-delimiter` changes the separator.

## Convert a trace with traceConv

Converting a text or csv trace to a binary format pays off quickly. The result replays up to about 10x faster with less memory, and the file is smaller. The `oracleGeneral` format also records when each object is requested next, which offline algorithms such as Belady need.

```bash
./bin/traceConv ../data/cloudPhysicsIO.txt txt --output-format=oracleGeneral
```

This writes `cloudPhysicsIO.txt.oracleGeneral` next to the input file. Use `-o <path>` to write it somewhere else. Supported output formats are `oracleGeneral` and `lcs` (with variants `lcs_v1`, `lcs_v2`, `lcs_v3`). If you leave out `--output-format`, you get `lcs`.

### Convert a csv trace

Tell the reader which columns hold the time, object id and size. Columns count from 1. If the object ids are numbers, add `obj-id-is-num=1`. The reader then uses the ids as they are instead of hashing them, which also uses less memory.

```bash
./bin/traceConv ../data/cloudPhysicsIO.csv csv \
    -t "time-col=2,obj-id-col=5,obj-size-col=4,obj-id-is-num=1" \
    --output-format=oracleGeneral
```

A trace that uses spaces as the delimiter:

```bash
./bin/traceConv /disk/trace.csv csv \
    -t "time-col=1,obj-id-col=2,obj-size-col=3,delimiter= ,obj-id-is-num=1" \
    --output-format=oracleGeneral
```

Every trace type that `cachesim` reads can be converted, including csv.

### Make a smaller sample

`-s` keeps a fraction of the *objects*, not of the requests. This is called spatial sampling: every request to a kept object stays in the trace, and every request to a dropped object goes.

```bash
# keep 1% of the objects
./bin/traceConv ../data/cloudPhysicsIO.vscsi vscsi -s 0.01 --output-format=oracleGeneral
```

### Things to know about conversion

* **Decompress `.zst` input first.** Conversion reads the trace backwards to compute next-access times, which compressed files do not allow. Simulation still reads `.zst` files directly.
* **Zero-size requests are dropped** before next-access times are computed, and conversion reports how many it dropped. This matters if you use Belady on older oracle traces. See [oracle traces and zero-size requests](oracle_traces.md).

## Simulate a cache layer with traceFilter

traceFilter models a two-layer cache hierarchy. It runs the trace through a first-layer cache and writes out only the requests that missed. That output is the workload the second layer sees, in `oracleGeneral` format.

```bash
# first layer: FIFO, sized at 1% of the working set, every object counted as size 1
./bin/traceFilter ../data/cloudPhysicsIO.vscsi vscsi --filter-type fifo --filter-size 0.01 --ignore-obj-size 1
```

```text
write 96518/113872 0.8476 requests to file cloudPhysicsIO.vscsi.filter_fifo_489.oracleGeneral
```

The output file lands in the current directory. Its name records the filter type and the cache size it resolved to (489 objects here).

| Option | Meaning |
| --- | --- |
| `--filter-type` | Eviction algorithm of the first layer, for example `fifo` or `lru` |
| `--filter-size` | First-layer cache size. Either an absolute size, or a fraction of the *working set size* (the total size of all distinct objects in the trace), such as `0.01` for 1% |
| `-o` | Output path |

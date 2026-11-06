# cachesim: run cache simulations

`cachesim` replays a trace (a recorded list of requests) through a simulated cache and tells you how often the cache missed. Use it to answer questions like "how big does my cache need to be?" or "would S3-FIFO beat LRU on this workload?" without writing any code.

It knows dozens of eviction algorithms, reads many trace formats, and runs several simulations in parallel.

## Before you start

[Build libCacheSim](install.md) first. `cachesim` ends up in `_build/bin/`.

Every command on this page is run from the build directory, `_build/`, so the sample traces in [data/](/data/) are at `../data/`:

```bash
cd _build
```

> [!WARNING]
> The sample traces are tiny. They are good for trying commands, but far too small to compare algorithms. See [cache_dataset](https://github.com/cacheMon/cache_dataset) for real ones.

## Your first simulation

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
```

The four arguments are always in this order:

| Argument | Example | Meaning |
| --- | --- | --- |
| trace path | `../data/cloudPhysicsIO.vscsi` | the trace file, optionally zstd-compressed (`.zst`) |
| trace type | `vscsi` | the file format, see [Using your own trace](#using-your-own-trace) |
| algorithm | `lru` | the eviction algorithm, case-insensitive |
| cache size | `1gb` | how big the cache is |

After a few log lines, you get the result:

```
../data/cloudPhysicsIO.vscsi LRU cache size     1GiB,           113872 req, miss ratio 0.6297, byte miss ratio 0.7274, throughput 4.62 MQPS
```

* **miss ratio** is the fraction of requests that missed the cache.
* **byte miss ratio** is the same thing weighted by object size, which is what matters if you care about bandwidth.
* **throughput** is how fast the simulator ran, in millions of requests per second.

Results are also appended to `result/<trace name>.cachesim` in the current directory. Use `-o` to pick another path.

Run `./bin/cachesim --help` to see every option.

## Choosing cache sizes

Write sizes with a unit and **no space**: `1gb`, `256mb`, `100m`. Units are not case-sensitive.

Give several sizes, separated by commas, and they run in parallel:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1mb,16mb,256mb,8gb

# spaces are fine if you quote the list
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru "1mb, 16mb, 256mb, 8gb"
```

You often don't know what sizes make sense for a new trace. Two options help:

**Use a fraction of the working set.** The working set size is the total size of all distinct objects in the trace. A number between 0 and 1 means "this fraction of the working set":

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 0.001,0.01,0.1,0.2
```

**Let cachesim pick.** With `auto` (or `0`), cachesim measures the working set and simulates at 0.1%, 0.3%, 1%, 3%, 10%, 20%, 40% and 80% of it. Sizes that would hold fewer than 100 objects are skipped.

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru auto
```

**Count objects instead of bytes.** With `--ignore-obj-size 1`, every object has size 1, so the cache size becomes a number of objects:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1000,16000 --ignore-obj-size 1
```

## Comparing algorithms

List several algorithms, separated by commas. cachesim runs every algorithm at every size, in parallel:

```bash
# 4 algorithms at one size
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi fifo,lru,arc,qdlp 0.01 --ignore-obj-size 1

# 4 algorithms x 4 sizes = 16 simulations
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi fifo,lru,arc,qdlp 0.01,0.05,0.1,0.2 --ignore-obj-size 1
```

At most `--num-thread` simulations run at once. The default is the number of CPU cores.

A few more examples:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi s3fifo auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi sieve auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lecar auto
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lhd auto
```

The [README](/README.md#supported-algorithms) lists every algorithm and the exact name to pass here. A few (GLCache, LRB, 3LCache, S4-FIFO) need an [optional build flag](install.md#optional-algorithms). If you ask for one that wasn't compiled in, cachesim stops with `do not support algorithm <name>`.

### Belady (the offline optimum)

`belady`, `beladySize` and `beladyCompute` cheat: they look into the future to decide what to evict. That makes them a useful lower bound, but they need a trace that records when each object is requested next. In practice that means an `oracleGeneral` trace:

```bash
./bin/cachesim ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral beladySize auto
```

Use `beladySize` when objects have different sizes, and `belady` with `--ignore-obj-size 1`. You can make an oracle trace from any other trace with [traceConv](quickstart_traceUtils.md). If your oracle trace contains zero-size requests, read [oracle traces and zero-size requests](oracle_traces.md) first.

## Using your own trace

cachesim reads these trace types:

| Type | What it is |
| --- | --- |
| `csv` | a text file with one request per line and comma- (or other) separated columns |
| `txt` | a text file with one object id per line, nothing else |
| `binary` | any fixed-size binary record, described with a format string |
| `oracleGeneral` | libCacheSim's compact binary format, which includes next-access times |
| `lcs` | libCacheSim's own binary format, with a header that stores trace statistics |
| `vscsi`, `twr` | two specific binary formats used by the sample traces |
| `syn` | no file at all: a generated uniform or Zipf workload, see [synthetic traces](synthetic_traces.md) |
| `lcp` | LLM prefix-cache traces, see the [lcp format](https://github.com/cacheMon/PrefixSim/blob/main/doc/lcp_format.md) |

Any of them can be zstd-compressed (`.zst`); cachesim reads them without decompressing first.

### csv traces

Tell cachesim which column holds what with `-t` (`--trace-type-params`). Columns are numbered from 1. The sample `cloudPhysicsIO.csv` has the header `version,time,op,size,lbn`, so:

```bash
./bin/cachesim ../data/cloudPhysicsIO.csv csv lru 1gb \
    -t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true"
```

The settings you will use most:

| Key | Meaning |
| --- | --- |
| `obj-id-col` | column with the object id (required) |
| `time-col` | column with the timestamp, in seconds |
| `obj-size-col` | column with the object size, in bytes |
| `obj-id-is-num` | `true` if the ids are numbers |
| `delimiter` | the separator, e.g. `delimiter=,` or `delimiter=\t` |
| `has-header` | `true` if the first line is a header |

Time and size are optional, but many algorithms behave differently without them, so include them if your trace has them.

cachesim guesses the delimiter and whether there is a header. If it guesses wrong, set them yourself. Note the doubled comma in `delimiter=,,`: the first comma is the value, the second separates it from the next key.

```bash
./bin/cachesim ../data/cloudPhysicsIO.csv csv lru 1gb \
    -t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true, delimiter=,, has-header=true"
```

**About `obj-id-is-num`.** By default cachesim treats ids as strings and hashes them. If the id column holds numbers, set `obj-id-is-num=true`, which is faster and uses less memory. cachesim notices numeric ids and refuses to guess:

```
[ERROR] csv.c: detect obj_id is numeric, please specify -t 'obj-id-is-num=1'
```

csv files must be plain ASCII; UTF-8 is not supported.

### txt and binary traces

```bash
# txt: one object id per line
./bin/cachesim ../data/cloudPhysicsIO.txt txt lru 1gb

# binary: describe each record with a Python struct-style format string,
# then say which field (counting from 1) is the id and which is the size
./bin/cachesim ../data/cloudPhysicsIO.vscsi binary lru 1gb \
    -t "format=<IIIHHQQ,obj-id-col=6,obj-size-col=2"

# oracleGeneral: time, id, size, and next access time
./bin/cachesim ../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral lru 1gb
```

The format string follows [Python's struct module](https://docs.python.org/3/library/struct.html): `<` means little-endian, `I` is a 4-byte unsigned int, `H` 2 bytes, `Q` 8 bytes.

> [!TIP]
> Binary traces are several times faster to replay than csv and use less memory. If you will run a trace more than once, convert it to `oracleGeneral` with [traceConv](quickstart_traceUtils.md) first.

## Tuning an algorithm

Many algorithms have parameters. Pass them with `-e` (`--eviction-params`) as `key=value` pairs separated by commas:

```bash
# SLRU with 4 segments
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi slru 1gb -e n-seg=4
```

To see what an algorithm accepts and its defaults, pass `-e print`:

```bash
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi slru 1gb -e print
# current parameters: n-seg=4,seg-size=25:25:25:25
```

## Admission and prefetching

An **admission** policy decides whether a missed object is worth inserting at all. Add one with `-a`:

```bash
# skip objects the first time they are seen, using a Bloom filter
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -a bloomFilter
```

Available: `bloomFilter`, `prob` (admit with a fixed probability), `size` (reject large objects), `sizeProbabilistic`, and `adaptSize`. Pass their parameters with `--admission-params`, for example `--admission-params="prob=0.8"`.

A **prefetching** policy loads objects before they are requested. Add one with `-p`:

```bash
# Mithril learns which objects tend to be accessed together
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -p Mithril
```

Available: `Mithril`, `OBL`, and `PG`. Pass their parameters with `--prefetch-params`.

## Other useful options

| Option | What it does |
| --- | --- |
| `--num-thread=4` | run at most 4 simulations at once |
| `--num-req=1000000` | stop after this many requests, handy for quick tests |
| `--warmup-sec=3600` | use the first hour to warm the cache and leave it out of the results |
| `--ignore-obj-size=true` | treat every object as size 1 |
| `--consider-obj-metadata=true` | count each algorithm's per-object metadata against the cache size |
| `--use-ttl=false` | ignore TTLs in the trace (only matters in a `-DSUPPORT_TTL=ON` build, see below) |
| `--hashpower=16` | size of the internal hash table, as a power of 2 (default 24, about 16M entries) |
| `--print-head-req=false` | don't print the first few requests at start-up |
| `-o my-output` | write results to `my-output` instead of `result/<trace>.cachesim` |
| `--verbose=0` | print only the results, no log lines |

A few of these deserve a closer look.

**Warmup.** `--warmup-sec` is measured in trace time, so it has to be shorter than the trace. The sample trace covers two hours, so `--warmup-sec=86400` (one day) uses up the whole trace and reports `0 req`.

**TTLs.** A build configured with `-DSUPPORT_TTL=ON` honors the TTLs in the trace by default, and `--use-ttl=false` replays the trace as if it had none. A normal build ignores TTLs and rejects `--use-ttl=true`.

**Hash table size.** The default `--hashpower=24` reserves room for about 16 million objects up front. On a small trace, lowering it saves a lot of memory. Replaying the sample trace drops from about 106 MB to 8 MB at `--hashpower=16`.

> [!NOTE]
> Sampling-based algorithms such as `RandomLRU` and `hyperbolic` draw eviction candidates from the hash table, so their miss ratios shift slightly with `--hashpower`. Keep it fixed when you compare results.

## Next steps

* [Analyze a trace](quickstart_traceAnalyzer.md) to understand why algorithms behave the way they do.
* [Build miss ratio curves quickly](quickstart_mrcProfiler.md) on very large traces.
* [Try your own algorithm](quickstart_plugin.md) without changing libCacheSim, or [add one to the library](advanced_lib_extend.md).

# Oracle traces and zero-size requests

An *oracle trace*, such as `oracleGeneral` or `lcs`, stores one extra field per request: `next_access_vtime`. It records when the same object will be requested next, and offline algorithms such as Belady and BeladySize rely on it to look into the future.

This page covers one way that field can go wrong: zero-size requests. If you run Belady or BeladySize on an oracle trace that contains zero-size records, and especially on one converted with an older libCacheSim, read on. Everyone else can skip this page. Background is in [issue #361](https://github.com/1a1a11a/libCacheSim/issues/361).

## The short version

* libCacheSim's readers drop zero-size requests by default.
* In oracle traces made by older versions, a kept request can say "my next access is request N" when request N was one of those dropped zero-size records. Belady then makes decisions on wrong information.
* Current libCacheSim detects this and refuses to run Belady or BeladySize on such input.
* To fix a trace, convert it again with the current `traceConv`. That gives you a new file with correct next-access positions.
* If you published or compared Belady or BeladySize results on affected traces, rerun them.

FIFO, LRU and other algorithms that don't look at future information are not affected.

## What `next_access_vtime` means

`next_access_vtime` is the 1-based position of the next request to the same object, counted in the stream the field was computed from. If there is no next request, it is -1 or `INT64_MAX`.

The key point: it must be computed from **the stream you actually simulate**. If requests are removed after the positions were computed, the positions no longer line up.

## Why dropping zero-size requests breaks Belady

The default reader drops zero-size requests. It does this even with `--ignore-obj-size=1`, because the filtering happens before sizes are changed to one.

In an older oracle trace, the positions were computed with the zero-size records still in place. After the reader drops them:

* **Belady** can see a request whose next access points at a dropped record. That object keeps a stale priority and may stay pinned in the cache indefinitely.
* **BeladySize** is affected too. Its score is `size * (next_access_vtime - current_request_count)`, so every position after a dropped record is off. Patching the broken links is not enough; all positions have to be recomputed.

## What libCacheSim does about it now

* Readers print a warning, once per reader, when they filter records out of an oracle trace.
* `cachesim` and the MINISIM profiler in `mrcProfiler` refuse to run Belady or BeladySize on such input. This includes compressed traces and zero-size records at the very end of the trace.
* The library's multi-cache simulator turns on the same check for these algorithms.
* If you write your own replay loop in C, set `reader_init_param_t.require_valid_next_access = true` before opening the reader to get the same check.

The check catches unsafe zero-size filtering. It does not validate oracle metadata in general, so it will not catch a trace that was malformed or already filtered in some other way.

## Repair a trace

Keep the original file. Create a new, separately named file with the current `traceConv`. It drops zero-size requests **first**, then computes next-access positions over the requests that remain.

```bash
# traceConv reads the input backwards, so decompress first.
# (Simulation itself still reads .zst files directly.)
zstd -d input.oracleGeneral.bin.zst -o original.oracleGeneral.bin

./bin/traceConv original.oracleGeneral.bin oracleGeneral \
    --output-format=oracleGeneral -o repaired.oracleGeneral.bin

# check the result: in object-count mode, Belady should not miss more than FIFO or LRU
./bin/cachesim repaired.oracleGeneral.bin oracleGeneral belady,fifo,lru \
    16982 --ignore-obj-size=1 --num-thread=3
```

`traceConv` prints how many zero-size records it removed. On the clean sample trace, that looks like:

```text
traceConv: filtered 0 zero-size records before recomputing next_access_vtime
```

The repaired file keeps the workload that FIFO and LRU already saw by default. The request count, timestamps, object ids and positive sizes are unchanged. It also handles an object that reappears after one or more zero-size records. Converting to `lcs` follows the same order.

### Practical notes

* **Disk space.** You need room for the decompressed input, the output, and a temporary file used by the reverse pass. The reverse pass also keeps a map whose size grows with the number of distinct objects kept.
* **Empty results are rejected.** An input with no requests left after filtering is an error.
* **Keep records.** Alongside benchmark results, save the input and output checksums, the libCacheSim commit, the conversion options, and the number of requests kept.
* **Republishing.** Use versioned file names and a manifest instead of silently replacing published files.
* **Start from the full stream.** Reconversion must start from a trace that still holds the requests you intend to simulate. If a file was already filtered but still carries the old positions, the reader's check cannot tell that records are missing. Reconverting still fixes it, because `traceConv` recomputes every position from scratch and ignores the old ones.

## Keeping zero-size requests instead

You can tell the reader to keep zero-size records with the reader option `-t ignore-size-zero-req=false`. This works for simulation and for conversion. In object-count mode (`--ignore-obj-size=1`), each one becomes an ordinary size-one request that counts toward requests and misses.

This keeps the original oracle positions valid, but it **changes the workload**, so treat it as a deliberate choice of what to study.

Also, don't assume a zero-size record is a delete. The [dataset description](https://github.com/cacheMon/cache_dataset#meta-key-value-cache-traces) says a zero size can mean a cache miss, and some source formats record DELETE operations separately. `oracleGeneral` has no operation field, so the size alone cannot tell you it was a delete. Keeping a zero-size record does not model invalidation either. If you want to study deletes, start from a source format that records operations and define the simulation model yourself.

## Should you rerun your benchmarks?

Rerun Belady and BeladySize results whenever the oracle stream contained zero-size requests that the reader skipped. A ranking that still looks reasonable does not prove the numbers are right.

* Start with the Meta key-value traces named in #361.
* Then check the *complete* stream of every other dataset you used. A zero-free beginning does not mean the whole trace is clean.
* FIFO and LRU results on the default (filtered) stream do not need rerunning just because the oracle fields changed. Any comparison or claim made *against* an affected Belady or BeladySize baseline does.
* Traces that were never filtered and have valid oracle metadata are not affected.

### A small worked example

Here is an independently reproduced 22-record trace, simulated at capacity two in object-count mode:

| Stream | Requests | Belady misses | FIFO/LRU misses |
| --- | ---: | ---: | ---: |
| Old reader: drop zero-size, keep original oracle | 21 | 21 (1.0000) | 3 (0.1429) |
| Keep zero-size records as size-one requests | 22 | 3 (0.1364) | 3 (0.1364) |
| Drop zero-size, recompute and rebase oracle | 21 | 3 (0.1429) | 3 (0.1429) |

With the broken oracle, Belady, which should be the best possible policy, misses on every request.

The full production-trace miss ratios reported in #361 have not been independently reproduced by this fix.

## Related limits

* Belady is only optimal when all objects are the same size. For variable-size objects, BeladySize is a heuristic that samples candidates.
* BeladySize has a separate accuracy limit under spatial sampling, described in the [mrcProfiler guide](quickstart_mrcProfiler.md#belady-and-beladysize).

# Oracle traces and zero-size requests

`next_access_vtime` is the **1-based record position of the next request to the
same object in the stream used to compute it**, or -1 / INT64_MAX if there is no
next request. The oracle must describe the stream actually simulated. See
[issue #361](https://github.com/1a1a11a/libCacheSim/issues/361).

## Why filtering matters

The default reader omits zero-size requests, including with
`--ignore-obj-size=1`: filtering happens before object sizes are changed to one.
In older oracle traces, a retained request can point to an omitted zero-size
request. Belady then keeps a stale priority, potentially pinning that object
indefinitely. BeladySize is affected too: its score uses
`size * (next_access_vtime - current_request_count)`, so simply repairing links
without rebasing all positions is insufficient.

Readers warn once per reader when they filter an oracle trace. `cachesim` and
MINISIM reject this input for Belady/BeladySize, including compressed input and
zero-size records at EOF. The library's multi-cache simulator also enables this
guard for these policies. Applications with their own replay loop should set
`reader_init_param_t.require_valid_next_access = true` before opening the reader.
The guard detects unsafe zero-size filtering; it is not a general validator of
arbitrarily malformed or previously filtered oracle metadata.

## Repair an existing published trace

Preserve the original trace. Create a separately named derivative using the
current `traceConv`, which filters zero-size requests **before** computing next
accesses and assigns positions in the retained stream. This preserves the
default FIFO/LRU workload, request denominator, timestamps, IDs, and positive
sizes. It also handles a key that reappears after one or more zero-size records.

```sh
# Conversion needs reverse access, so decompress first. Ordinary simulation
# still supports .zst files directly.
zstd -d input.oracleGeneral.bin.zst -o original.oracleGeneral.bin
./bin/traceConv original.oracleGeneral.bin oracleGeneral \
    --output-format=oracleGeneral -o repaired.oracleGeneral.bin
./bin/cachesim repaired.oracleGeneral.bin oracleGeneral belady,fifo,lru \
    16982 --ignore-obj-size=1 --num-thread=3
```

Conversion also applies this ordering when producing LCS. Its existing reverse
pass uses a map proportional to the number of distinct retained objects and a
temporary reverse file. Allow disk space for the decompressed input, output,
and temporary file. Conversion reports how many zero-size records it filtered;
an input with no retained requests is rejected. Record input/output checksums,
the libCacheSim commit, conversion options, and retained request count alongside
benchmark results. If republishing, use versioned paths and a manifest rather
than replacing published files silently.

Reconversion must start from a stream containing the intended requests. If a
file was already filtered but still carries old timestamps, the missing records
cannot be detected by the reader guard; reconversion recomputes the remaining
stream's oracle without trusting the old pointers.

## Retaining zero-size records is a separate workload choice

The shared reader parameter `-t ignore-size-zero-req=false` explicitly retains
these records, for simulation or conversion. In object-count mode they become
ordinary size-one accesses and contribute to request and miss counts. This
keeps the original oracle positions consistent but changes the workload.

Do not interpret every zero-size record as a deletion. The
[dataset description](https://github.com/cacheMon/cache_dataset#meta-key-value-cache-traces)
states that zero size can indicate a cache miss; some source formats also carry
DELETE operations. `oracleGeneral` has no operation field, so deletion semantics
cannot be recovered from size alone. Retaining a zero-size record does not
implement invalidation, either. A study of deletion semantics must use the
operation-bearing source and define its simulation model separately.

## Benchmark revalidation

Revalidate Belady and BeladySize results whenever an oracle stream contained
zero-size requests that the reader skipped. A ranking that still looks sensible
does not establish correctness. Prioritize the Meta KV traces named in #361,
then audit the complete streams for every other dataset used; a zero-free prefix
does not establish that an entire trace is unaffected.

The independently reproduced 22-record example, at capacity two in object-count
mode, has these results:

| Stream | Requests | Belady misses | FIFO/LRU misses |
| --- | ---: | ---: | ---: |
| Old reader: filter zero, keep original oracle | 21 | 21 (1.0000) | 3 (0.1429) |
| Retain zero as a size-one access | 22 | 3 (0.1364) | 3 (0.1364) |
| Filter zero, recompute and rebase oracle | 21 | 3 (0.1429) | 3 (0.1429) |

Full production-trace miss ratios reported in #361 have not been independently
reproduced by this change. Existing FIFO/LRU results for the default filtered
stream do not require revalidation solely because the oracle fields changed.
Comparisons and claims relative to the affected offline baselines do.

Traces with no filtering and valid oracle metadata are unaffected by this bug.
Belady's paging optimality assumes equal-size objects; BeladySize remains a
sampled heuristic for variable-size objects. Its separate
[sampling-coordinate limitation](quickstart_mrcProfiler.md) remains in effect.

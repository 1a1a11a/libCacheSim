# prefixsim quickstart

`prefixsim` replays an LLM request trace through a KV-block prefix cache and
reports, for each eviction policy, the **block hit ratio** and the **compute
saving ratio** (prefill compute avoided, weighted by a cost model). Every block
of a request must be resident at once, as in a real serving engine. The design
is in [prefixsim.md](prefixsim.md); this page is how to run it.

---

## Build

```bash
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release
ninja -C _build
```

This builds `prefixsim` and the trace converter `traceConvLLM` in
`_build/bin/`. The commands below run from `_build/`.

---

## 1. Convert the trace to lcp

`prefixsim` reads the [lcp](lcp_format.md) format. Convert a JSONL trace once:

```bash
./bin/traceConvLLM --input qwen_traceA_blksz_16_pos.jsonl --output qwen_traceA.lcp
```

```
wrote qwen_traceA.lcp.zst: 43058 requests, 6293976 blocks, 2656400 unique, 0 sessions, 0 users, 4 types, max 5581 blocks/request
```

- The output is zstd-compressed, as `zstd -8 --long`, and `.zst` is appended to
  the name. `--no-zstd` writes a plain file instead.
- Each JSONL line is one request. `hash_ids`, the per-block hashes in prefix
  order, is required; the converter also reads `timestamp`,
  `timestamp_arrival`, `chat_id`, `parent_chat_id`, `session_id`, `user_id`,
  `input_length`, `output_length`, `turn` and `type` when present.
- The converter holds the whole JSONL trace in memory: 12.8 GiB and about
  2 minutes for fi-new (327,548 requests, 656M block accesses).
- Given an `lcp` file, the converter writes JSONL instead
  (`--input qwen_traceA.lcp.zst --output qwen_traceA.jsonl`). To take a slice
  of an `lcp` trace, filter that JSONL and convert it again; ids and next
  access are recomputed.

Check what the file holds:

```bash
./bin/tracePrint qwen_traceA.lcp.zst lcp --print-stat 1
```

```
trace stat: n_req 43058, n_block 6293976, n_unique_block 2656400, n_session 0, n_user 0, max 5581 blocks/request, 16 tokens/block
completion times 0 us to 7199464000 us, duration 7199 seconds 0.08 days
request types: text image search file
```

> [!WARNING]
> `prefixsim` also runs on the JSONL directly, but much more slowly and with
> far more memory. On the full fi-new trace, one LRU run takes 169 s and
> 12.9 GiB of memory from JSONL, against 103 s and 80 MiB from `.lcp` (105 s
> and 146 MiB from `.lcp.zst`); on chutes, 238 s and 15.4 GiB against 66 s and
> 78 MiB. A JSONL trace is parsed and held whole by every process, so a sweep
> that runs one policy per process needs that memory for each.

---

## 2. Run

```bash
./bin/prefixsim --trace qwen_traceA.lcp.zst --cache-size 8k \
    --algo lru,s3fifo,belady_compute --cost-model position
```

```
trace qwen_traceA.lcp.zst: 43058 requests, 6293976 block accesses, format lcp, block-id prefix
cache size 8192 blocks, cost model position

lru                      block hit ratio 0.1404   compute saving 0.0151
RESULT trace=qwen_traceA.lcp.zst algo=lru cache_size=8192 cost_model=position block_id=prefix n_req=43058 n_req_skipped=0 n_blocks=6293976 block_hit_ratio=0.140357 compute_saving_ratio=0.015117 n_eviction=5402382 n_self_eviction=0 n_unexpected_eviction=0 n_unobserved_eviction_round=0
s3fifo                   block hit ratio 0.1553   compute saving 0.0193
RESULT trace=qwen_traceA.lcp.zst algo=s3fifo ...
belady_compute           block hit ratio 0.2449   compute saving 0.2634
RESULT trace=qwen_traceA.lcp.zst algo=belady_compute ...
```

Each algorithm prints a summary line and a `RESULT` line of `key=value`
pairs for scripts; `--output <file>` also appends the `RESULT` lines to a file.

**Cache size** is in blocks, with `k`, `m`, `g` meaning ×1024. For scale: a
16-token block of Qwen3-Coder-30B's KV cache in bf16 is 1.5 MiB, so 1k blocks
is 1.5 GiB.

**Algorithms**: `--algo` takes a comma-separated list, run one after another;
`--list-algos` prints all 43, or 46 in a build with LRB, 3LCache and S4-FIFO
enabled. The oracles (`belady`, `belady_compute`, `session_belady`) use the
next-access times stored in the trace.
`--algo-params` passes algorithm-specific parameters, e.g.
`--algo-params "n-sample=64"`.

**Cost models** (`--cost-model`) set what a block costs to recompute, from its
0-based position `p` in its request:

| model | cost |
|---|---|
| `linear:A,B` | A + B · p, for any model: fixed per-block work plus attention over the context |
| `table:FILE` | line p+1 of FILE, for measured costs; positions past the end use the last line |
| `uniform` (default) | 1, so compute saving equals hit ratio |
| `position` | p + 1 |
| `qwen3coder30b_blksz_16` | 865 + 2 · p, measured on Qwen3-Coder-30B |

Costs are integers. The same strings work in `cachesim -t cost-model=<m>` on an
`lcp` trace.

---

## Reading the result

| field | meaning |
|---|---|
| `block_hit_ratio` | fraction of block accesses already cached |
| `compute_saving_ratio` | the same, weighted by block cost |
| `n_req_skipped` | requests with more distinct blocks than the cache; skipped and left out of both ratios |
| `n_eviction` | blocks evicted to make room |
| `n_self_eviction` | of those, blocks the arriving request itself needed |
| `n_unexpected_eviction` | evictions while replaying a request; **must be 0**, or the run is not trustworthy |

The other diagnostics are described in
[prefixsim.md](prefixsim.md#diagnostics-in-the-result-line).

---

## Other options

| option | effect |
|---|---|
| `--trace-format lcp\|qwen-jsonl` | input format; by default `lcp` if the file name contains `.lcp`, else JSONL |
| `--replay-order prefix-order` | replay a request from its first block instead of its deepest ([why](prefixsim.md#replay-order---replay-order)) |
| `--no-pin` | let eviction take blocks the arriving request already holds |
| `--no-verify` | skip the check that every block is resident after each request |
| `--dump-holes <dir>` | write each request's runs of missed blocks, per algorithm |
| `--block-id raw` | JSONL only: use the trace's hashes as block ids without positional encoding ([when](prefixsim.md#block-identity---block-id)) |

---

## Running many simulations

`prefixsim` runs the algorithms in `--algo` one after another, each streaming
the trace once. To use more cores, run one process per algorithm and cache
size: on an `lcp` trace each process needs only its cache plus tens of MB.

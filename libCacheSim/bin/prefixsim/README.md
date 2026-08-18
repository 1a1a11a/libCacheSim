# prefixsim — LLM prefix-cache simulator

`prefixsim` evaluates KV-block eviction policies on LLM serving traces. It
reports **block hit ratio** and **compute saving ratio** under the constraint
that a request needs *all* of its blocks resident at the same time — the thing
that makes a prefix cache different from an ordinary object cache, and the thing
libCacheSim's request-at-a-time model cannot express on its own.

```bash
./bin/prefixsim --trace qwen_traceA_blksz_16.jsonl --cache-size 8k \
    --algo lru,random_compute,belady_compute --cost-model position
```

`--cache-size` is in **blocks**, not bytes: every block is one object of size 1,
so a cache of 8k is 8192 blocks.

---

## The problem this solves

An LLM request is not one object, it is an ordered list of KV blocks
(`hash_ids` in the trace), index 0 being the prefix root. To serve the request
the engine needs every block materialised at once: the resident ones are reused,
the missing ones are prefilled into freshly allocated slots.

A normal cache simulator has no notion of "these N objects must coexist", so the
obvious workaround — the one `evaluate/evaluate.cpp` uses — is to free space for
the whole request before touching it:

```c
while (free_space < request_size) cache->evict(cache, req);
```

This **systematically understates the hit ratio**. The eviction loop is blind to
which request is arriving, so it happily evicts blocks that this very request
was about to reuse. Every such eviction converts a hit into a miss, and the
damage grows with request size and with cache pressure.

`prefixsim` fixes it by making the arriving request visible to the allocation
step, and by measuring *before* allocating.

---

## How a request is served

Three phases, in this order, with one hand-off in between. Call the arriving
request **Alpha**.

### Phase 1 — match (read-only, produces every statistic)

Probe each block of Alpha against the cache, one by one, in prefix order.

```
for i in 0 .. B-1:
    resident[i] = probe(block[i])
```

Three properties matter:

- **The probe is invisible to the eviction algorithm.** It calls
  `hashtable_find_obj_id(cache->hashtable, id)` directly, never
  `cache->find()`. Even `cache->find(..., update_cache=false)` is the
  algorithm's own code path, and phase 1 must not look like an access —
  otherwise a matched-but-not-yet-served block would get its recency or
  frequency bumped twice.
- **Every block is checked, not just the leading run.** A block at a higher
  index can be resident even when an earlier one is missing, and this simulator
  counts it as reusable — matching does not stop at the first miss.
- **All statistics are computed here**, against the cache exactly as Alpha found
  it. Phases 2 and 3 cannot change the reported numbers. That is what makes the
  measurement independent of how much churn serving the request causes.

### Phase 2 — allocate

Alpha needs `M` free slots, where `M` is the number of *distinct* missing
blocks. Some slots may already be free:

```
needed = M - (cache_size - occupied)
```

Now evict until `needed` slots have been reclaimed — but **only evictions of
blocks Alpha does not want count as progress**:

```
progress = 0
while progress < needed:
    victim = evict()
    if victim ∈ Alpha:  self_evicted += 1      # no progress
    else:               progress    += 1
```

Why the split: evicting a block Alpha is about to reuse frees one slot *and*
adds one block to the set Alpha must insert. Net progress is zero. The
arithmetic works out exactly — with `H` hits, `M` misses, `S` self-evictions and
`P` useful evictions, phase 3 must insert `M + S` blocks into
`(cache_size - occupied) + P + S` free slots, and the `S` cancels, leaving
`P = M - (cache_size - occupied)`. This is why the loop can ignore
self-evictions entirely rather than compensating for them.

**Termination.** A request with more distinct blocks than the whole cache can
never be resident, so it is skipped up front (counted as `n_req_skipped`). For
every other request the cache holds at least `needed` blocks Alpha does not
want, and each self-eviction permanently removes a block from the resident Alpha
set — there can be at most `H` of them before every subsequent victim counts.
The loop also hard-fails if a call to `evict()` removes nothing at all, rather
than spinning.

**Batched eviction.** The deficit is known up front, so when the algorithm
offers `cache->evict_n()` the loop asks for the whole remaining `needed -
progress` at once instead of one block per call. Sampling algorithms pay their
sampling cost per call, so this is the difference between drawing the sample once
per round and once per evicted block: on `qwen_coder` at 8k with 128 samples,
`partial_node_random_freq` runs 6x faster batched and lands on the same hit ratio
to four decimals. It also makes runtime nearly independent of the sample size, so
a large sample stops being expensive.

`needed - progress` is a hard cap. Self-evictions still do not count as progress,
so the loop may go round again -- that is correct, not a shortfall.

The partial-node algorithms take that batch in one of two modes, set with
`eviction-mode`:

| mode | chunk taken from the winning node |
|---|---|
| `drain` (default) | the whole remaining deficit |
| `micro` | at most `micro-batch` blocks (default 64), then re-sample and re-score |

On `qwen_coder` they land within 0.002 of each other at every cache size, so the
cheaper mode is the better default: `drain` runs in 3.8s against `micro`'s 4.3s
at batch 64, 8.3s at batch 8, and 37s at batch 1. Chunk size is close to free
here -- what matters is `n-sample`, where 128 buys ~0.9 points of hit ratio over
32 and saturates by 512.

**Knowing the victim.** The loop needs the identity of each evicted object.
`cache_evict_base()` already calls `prefetcher->handle_evict` on every genuine
eviction, so the simulator installs a do-nothing prefetcher that just records
victim ids. The tempting alternative — peek with `cache->to_evict()` before
calling `cache->evict()` — is **wrong**: `RandomCompute`,
`RandomQuickDemotion`, `BeladyCompute` and the other samplers re-sample inside
`evict()` instead of reusing `to_evict_candidate`, so the peek names a different
object than the one actually removed.

### Between allocate and access — `record_request`

Algorithms that evict at prefix-tree-node granularity cannot reconstruct a
request's path from individual accesses, so the simulator hands it to them
directly:

```c
if (cache_->record_request != nullptr) {
  cache_->record_request(cache_, request.blocks.data(), n_blocks);
}
```

`record_request` is an optional slot on `cache_t`, `NULL` for every algorithm
that does not need it (`cache_struct_init` zeroes the struct), so the check is
required. It is the C analogue of `FreeBlockManager.record_request_blocks()` in
the vLLM prototype, and it is deliberately called **here** rather than at init or
during replay:

- *After* allocate, because the algorithm must already know the path when it is
  asked to evict against it on the following request.
- *Before* access, because the blocks are not in the cache yet. The tree records
  them as not-yet-resident and phase 3's inserts promote them, so the tree's
  residency and the cache's contents never disagree.

Only the simulator can supply this — it is the only component that knows where
one request ends and the next begins.

### Phase 3 — access

Replay Alpha through the eviction algorithm with ordinary `cache->get()` calls,
so the policy sees the request the way it normally would. Because phase 2
reserved exactly the right number of slots, no eviction should occur here; any
that does is counted as `n_unexpected_eviction` and reported as a warning,
because it means the accounting and the algorithm disagree.

---

## Reverse-order replay

Blocks are replayed **deepest first**, from index `B-1` down to index `0`.

The prefix root is the most valuable block in the cache: it is shared by every
request that starts with the same prompt. The deepest block is the most private.
If a request were replayed in forward order, the root would be touched first and
end up nearest the LRU tail — recency-ordered policies would evict exactly the
block with the most reuse. Replaying in reverse leaves the root at the MRU end
and the deep, private blocks near the tail, so eviction eats the prefix from the
far end inward, which is both what a real engine wants and what keeps the
resident set prefix-contiguous.

Measured against replaying in prefix order, this is worth 0.001-0.004 of hit
ratio to the recency-ordered policies (LRU at 4k: 0.1625 reverse vs 0.1589
forward) and essentially nothing to the partial-node ones, which agree to five
decimals either way. That is mechanical: reverse replay works by arranging
per-block recency so the root ends up most-recently-used, and a node-granularity
policy does not use per-block recency to pick which block inside a node to drop
-- `evict-from` controls that instead. Worth knowing before assuming the trick
carries over to a new partial-node variant.

## Next-access annotation

Belady-style oracles read `req->next_access_vtime`. Those virtual times are
computed on the **forward** flattened trace (request 0 block 0 is vtime 1, block
1 is vtime 2, …), *not* on the reverse replay order.

This is deliberate and is the mirror image of the rule above. Within a request
the forward numbering makes the deepest block look like the one needed furthest
in the future, so an oracle evicts from the deep end — the same direction
recency policies are pushed by the reverse replay. Annotating in replay order
would invert it and make Belady evict the prefix root first.

Consequence to be aware of: `cache->n_req` advances in *physical* (reverse)
order while the annotation is in *logical* (forward) order. The two agree at
every request boundary and differ by at most `B` inside a request. That
in-request skew is the intended signal, not an error.

---

## Metrics

| Metric | Meaning |
|---|---|
| `block_hit_ratio` | Fraction of block accesses that found the block resident, wherever it sits in the prefix. |
| `compute_saving_ratio` | The same, weighted by `block_cost()`: saved cost over total cost. |

Both are measured in phase 1, so they describe the cache as each request found
it and are unaffected by the churn of serving that request.

## Cost models (`--cost-model`)

Cost is a function of the block's 0-based position in its request — the only
signal the qwen trace carries.

| Model | Cost | Notes |
|---|---|---|
| `uniform` (default) | `1` | Compute saving ratio equals block hit ratio. |
| `position` | `pos + 1` | Blocks that must be prefilled from the root to reconstruct this one. Matches the `compute` field written into `.lcsllm` traces. |
| `affine` | `863 + 2*(pos+1)` | Reproduces `compute_intensity_transform()` in the `evaluate/` harness. For comparison only — the large constant compresses the range so far (position 1 vs 100 is 865 vs 1063) that cost-aware policies rank nearly like cost-blind ones. |

## Block identity (`--block-id`)

- `prefix-hash` (default) — fold the whole prefix into a rolling
  `boost::hash_combine`, so a block's identity is *the path from the root up to
  and including it*. This is what a real prefix cache does. The hash is
  byte-identical to `llm_qwen.py`, so a trace converted to `.lcsllm` and the same
  trace read natively here produce the same identities.
- `raw` — use the trace's `hash_ids` verbatim. Only correct if the trace's ids
  are already prefix-unique. **The qwen traces are not**: on
  `qwen_traceA_blksz_16.jsonl`, 16,686 ids appear under more than one predecessor
  and 52,168 appear at more than one position, so `raw` merges distinct prefix
  paths into a single object and over-counts reuse (0.1450 vs 0.1404 for LRU at
  8k). It also makes a block's cost ambiguous, which with a position-dependent
  cost model trips the `cache_find_base()` cost-change `abort()`; the tool warns
  about that combination.

---

## Which algorithms work

The phase-1 probe reads `cache->hashtable` directly, so an algorithm is usable
only if its entire resident set lives there. `--list-algos` prints the
allowlist: `lru`, `fifo`, `clock`, `sieve`, `lfu`, `lfuda`, `mru`, `size`,
`random`, `hyperbolic`, `gdsf`, `gdsf_compute`, `belady`, `belady_compute`,
`random_compute`, `random_quick_demotion`, `partial_node_random_compute`.

The two partial-node algorithms sample prefix-tree *nodes* rather than blocks and
evict out of the winning node, leaving the rest of that node cached:

| algorithm | score |
|---|---|
| `partial_node_random_compute` | `cost / recency` — same as `random_compute` |
| `partial_node_random_freq` | `(freq+1) * cost / recency` — reproduces `RandomFreeBlockManager` ("Random") in the vLLM prototype |

Both are kept because the frequency term is regime-dependent: dropping it costs
4-6 points of hit ratio on `qwen_coder` at 4k-20k blocks, but is neutral-to-
positive once the cache is large enough that the hit ratio clears ~0.7. Measure
before choosing.

Tune with `--algo-params "n-sample=128,evict-from=tail"`; see
`eviction::PartialNodeCache` for how to add a variant — a new one needs only a
`score()` override.

Composite algorithms are excluded on purpose. The S3FIFO family, `TwoQ`,
`WTinyLFU`, `ARC` and `LIRS` keep objects in sub-caches with their own hash
tables, or keep ghost entries in the main one, so a direct probe would report
the wrong residency in both directions.

As a backstop, after every request the simulator re-probes each block and fails
loudly if one is missing (disable with `--no-verify`). That check catches an
unsupported algorithm immediately instead of letting it produce quietly wrong
numbers.

## Diagnostics in the `RESULT` line

| Field | Meaning |
|---|---|
| `n_req_skipped` | Requests with more distinct blocks than the cache; excluded from all ratios. |
| `n_eviction` | Objects evicted during allocation. |
| `n_self_eviction` | …that the arriving request itself needed. High values mean the policy keeps throwing away what it is about to re-read; this is the cost the naive free-space-first approach pays invisibly. |
| `n_unexpected_eviction` | Evictions during replay. **Must be 0.** Anything else means the allocation arithmetic and the algorithm disagree and the run is untrustworthy. |

## Adding an input format

Implement `TraceReader` in `trace.cpp` and add one line to
`parse_trace_format()` and `open_trace()`. The simulator only ever sees
`Request`, so nothing else changes.

This does not reuse libCacheSim's `reader_t` because a `reader_t` emits one
`request_t` (one object) at a time and cannot express "these N objects form one
request that must be resident together".

## Known limitations

- **No eviction dependency between blocks.** Blocks are independent objects; a
  real radix cache cannot keep a child whose parent has been evicted. Nothing
  here enforces that, which is why a block at a higher index counts as a hit
  even when an earlier one is missing.
- **No time model.** `timestamp` is carried into `req->clock_time` but replay is
  request-ordered; there is no batching, scheduling, or concurrency.
- **Blocks of an in-flight request are not pinned.** A real engine refcounts
  them so they cannot be evicted mid-service. Here they can be, which is what
  `n_self_eviction` counts; the statistics are unaffected because they are taken
  in phase 1, but the extra churn is real. Pinning would be the natural next
  refinement.
- **The whole trace is held in memory** (block ids plus next-access times, 16
  bytes per block access) so next-access annotation can be a single pass.

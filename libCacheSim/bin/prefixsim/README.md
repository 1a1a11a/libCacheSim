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
never be resident, so it is skipped up front (counted as `n_req_skipped`, and
warned about once — see below). For
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
`RandomComputeSmallQueue`, `BeladyCompute` and the other samplers re-sample inside
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

### The other hook — `set_request_ctx`

`record_request` says *which blocks*; `set_request_ctx` says *what kind of
request*, and it fires at the top of `serve()`, before phase 1:

```c
if (cache_->set_request_ctx != nullptr) {
  cache_request_ctx_t ctx{request.timestamp, request.category, n_blocks};
  cache_->set_request_ctx(cache_, &ctx);
}
```

Two hooks rather than one, because the useful moment differs. `record_request`
has to run after allocate (above); but a policy that decays by wall-clock time
needs the *current* "now" while it is choosing victims, and a policy that fits a
separate reuse-time distribution per workload class needs to know which class the
arriving request belongs to. Delivering either through `record_request` would lag
it by exactly one request. So the properties of the request travel ahead of it,
and its block path follows behind — matching `set_request_context()` in the vLLM
prototype's harness, which is placed the same way for the same reason.

`ctx.timestamp` is a `double` in seconds. `request_t::clock_time` would do the job
except that it truncates to whole seconds, and with a KV-block lifespan of order
100 s that would collapse every block arriving within the same second into a tie.

`ctx.category` is an opaque id, folded here from the trace's `type` and `turn`
fields — the (request type, turn number) pair that arXiv:2506.02634 calls a
request category. Opaque on purpose: what makes two requests the same class is a
property of the trace, so the schema knowledge stays in the trace reader and the
algorithm only ever tests ids for equality. `0` means the trace carried neither
field, which a consumer must read as "one undifferentiated class".

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
| `qwen3coder30b_blksz_16` | `865 + 2*pos` | Measured per-block prefill cost of Qwen3-Coder-30B at 16-token blocks: a fixed part (projections, MLP) plus a part growing with the context attended to. Matches the vLLM logs (`idx=0→865, idx=100→1065`) and `compute_intensity_transform()` in `evaluate/`. Narrow range — position 1 vs 100 is 865 vs 1065 — so cost-aware policies rank close to cost-blind ones under it. |

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

The option exists because this reader takes *raw JSONL*, where the convention is
a property of the file and cannot be assumed. `.lcsllm` is the opposite: it
guarantees positional encoding, a reader rejects a file claiming otherwise, and
there is nothing for a consumer to choose — see
[the format spec](/doc/lcsllm_format.md#positional-encoding-of-block-ids).

`raw` is worth keeping for one specific job: reproducing the vLLM prototype,
which keys on `hash_ids` directly. That is why the freeinference comparison above
uses it. On a trace whose ids already *are* prefix-unique the two modes agree
exactly — LRU at 120k blocks on `prod_trace_w4_multiturn_concurrency_1024` gives
0.178042 / 0.090094 under both — so `prefix-hash` is never the riskier choice;
`raw` is only ever the one that can be wrong.

---

## Requests larger than the cache

Such a request cannot be served the way this simulator defines serving — every
block resident at once — so it is skipped: it contributes nothing to either
ratio, and it does not touch the cache, so it cannot evict anything on its way
past. `n_req_skipped` reports how many.

That is worth a warning, because it silently narrows what the reported ratios
average over: at a cache size below the largest prompt in the trace, the hardest
requests are exactly the ones dropped, which flatters every policy equally. The
warning names the first offender and its block count, so the cache size can be
compared against it:

```
[WARN] request 1 needs 4 distinct blocks, more than the whole cache (3);
       skipping it and excluding it from every ratio. Further oversized
       requests are skipped silently -- see n_req_skipped.
```

It fires **once per run**, via libCacheSim's `WARN_ONCE`. Once, because being
oversized is a property of the cache size rather than of the individual request:
choose a small enough cache and a whole class of requests qualifies, which at
trace scale would bury every other line of output. The exact count is in the
`RESULT` line either way. It is also once across all algorithms in a
comma-separated `--algo` list, which is correct — the skipped set depends only on
the trace and the cache size, so it is identical for every policy.

If `n_req_skipped` is more than a rounding error, the run is answering a
different question than intended; raise `--cache-size` or accept that the
comparison excludes the largest prompts.

---

## Which algorithms work

39 algorithms -- everything `cachesim` offers except one. `--list-algos` prints
them. The list is verified rather than assumed: an entry is there because it
completes a whole trace with the post-request residency check on.

Getting there needed two changes here and several in the algorithms themselves:

- **Residency is asked of the algorithm**, via `cache->find(req, update_cache =
  false)`, not read out of `cache->hashtable`. An algorithm's resident set is not
  always its main hash table: the S3FIFO family keeps blocks in sub-caches with
  their own tables, so a raw probe reports them missing, while ARC and LIRS keep
  ghost entries in the main table, so a raw probe reports those as hits. Because
  `cache_find_base` gates every mutation on `update_cache`, asking the algorithm
  still records nothing.
- **Allocation does not require victim identity.** It uses it when available --
  the eviction hook makes the self-eviction split cheap -- but composite
  algorithms evict inside sub-caches whose prefetcher is not ours, so no victim
  is reported. On that path one re-probe of the request recovers both the
  remaining deficit and the exact self-eviction count, so nothing is lost but
  speed. `n_unobserved_eviction_round` says when it was used.
- **Several `evict()` implementations did not free anything.** `evict()` that
  only promotes a block from a small queue to a main queue leaves occupancy
  unchanged; `cache_get_base` hides this by looping until it has room. Fixed in
  `S3FIFOd`, `QDLP`, `S3FIFOCompute`, `LIRS` and `CAR`, each verified to leave
  its cachesim miss ratio unchanged.

### Published prefix-cache policies

Three entries reimplement policies from the literature, so that a new idea can be
measured against what has already been proposed rather than only against generic
cache algorithms.

| `--algo` | Paper | Signal |
|---|---|---|
| `workload_aware` | arXiv:2506.02634, *KVCache Cache in the Wild* (ATC'25) §4.2 | per-workload-class reuse probability, then prefix offset |
| `asym_cache` | arXiv:2606.02964, *Multi-Segment Attention* §4.2-4.5 (the paper's MSA) | expected recompute cost, block-count clock |
| `asym_cache_time` | same, with the paper's wall-clock lifespan | expected recompute cost, seconds clock |

Both files carry the derivation in their header comment; two things are worth
knowing before reading a number off them.

**`asym_cache` needs a position-dependent cost model.** Its `dT_B` is the paper's
Eq. 7, linear in the block's prefix position, which is exactly what
`--cost-model qwen3coder30b_blksz_16` computes. Under `--cost-model uniform`,
`dT_B` is constant and the policy reduces to LRU — the paper says so itself
("if the recovery cost of all cache blocks is a uniform constant, our algorithm
degrades to the conventional LRU strategy"), so that combination measures nothing.

**The clock is the whole story for `asym_cache` vs `asym_cache_time`.** They differ
in one line and separate by 8 points of compute savings at 500k blocks. The paper
defines the reuse-time distribution on seconds and sets the lifespan `L` to its
P99; fit `L` on a *block* clock instead and it lands tens of millions of blocks
out, beyond any cache, so `tau/alpha ~ 0` for everything resident, `f_B` goes flat
and the policy collapses to cost-weighted LRU. `asym_cache` is kept precisely
because that collapse is worth being able to show.

Both need per-request metadata, so they install `set_request_ctx` as well as
`record_request`. Under a driver that calls neither — plain `cachesim`, the test
suite — they fall back to a per-access clock and a single workload class, which is
enough to keep them correct but throws away what makes them interesting.

### The one exclusion: admission control

`clockpro` is excluded, and the reason is about prefix caches rather than about
this simulator.

In an ordinary object cache, declining to admit an object is harmless. The client
already has its data -- it came from the backing store -- and the cache has
merely chosen not to keep a copy. Admission control is a cheap and legitimate
policy lever there, and ClockPro uses one: `can_insert` refuses once
`mem_cold + size > mem_cold_max`.

A prefix cache has no backing store to read from. The cached "data" is KV state,
and the model can only attend over blocks that physically exist in GPU memory.
Serving a request therefore *requires* materialising every one of its blocks: a
missing block is prefilled and written into HBM, not fetched from anywhere. The
block is resident during service whether the policy wanted it or not.

So the only decision a prefix-cache policy actually gets to make is what to keep
*after* the request completes. "Refuse to store this block" has no analogue at
the moment of use; the nearest expressible thing is to evict it immediately
afterwards, which is exactly what a small admission queue or a quick-demotion
penalty does -- and both of those are supported.

That is why the post-replay check requires every block of the request to be
resident. It is not a strictness setting, it is the modelling invariant, and an
algorithm that can refuse admission is describing a cache this simulator does not
model. ClockPro's crashes and hangs under prefixsim *were* fixed -- an infinite
loop, a stack overflow and a hash-table assertion, all with its cachesim miss
ratio unchanged -- so what remains is only this semantic mismatch. Relaxing the
check to tolerate a refused `can_insert` would admit it, at the cost of prefixsim
no longer guaranteeing that a served request was fully resident.

## Diagnostics in the `RESULT` line

| Field | Meaning |
|---|---|
| `n_req_skipped` | Requests with more distinct blocks than the cache; excluded from all ratios, and left untouched (a skip evicts nothing). Warned about once per run. |
| `n_eviction` | Objects evicted during allocation. |
| `n_self_eviction` | …that the arriving request itself needed. High values mean the policy keeps throwing away what it is about to re-read; this is the cost the naive free-space-first approach pays invisibly. |
| `n_unexpected_eviction` | Evictions during replay. **Must be 0.** Anything else means the allocation arithmetic and the algorithm disagree and the run is untrustworthy. |

## Dumping holes (`--dump-holes`)

A *hole* is a contiguous run of blocks a request had to recompute — a gap in
what the cache could give it. Where the hit ratio says how much was reused, the
hole distribution says how that reuse was *shaped*: one long tail to prefill is a
very different workload for the engine than a prefix shot through with gaps, even
at the same hit ratio.

```bash
prefixsim --trace qwen_coder_blksz_16_pos.jsonl --cache-size 8k \
          --algo lru,partial_node_random_compute --dump-holes holes_logs/
```

One file per algorithm:

```
holes_logs/holes_qwen_coder_blksz_16_pos_cache8192_lru.txt
```
```
Request 0: (idx: 0, len: 63)
Request 1: (idx: 1, len: 273)
Request 2: (idx: 1, len: 258)
```

`idx` is the 0-based block position within the request, `len` the run length.
Both the line format and the file name are fixed by
`evaluate/analyze_holes.py`, which recovers dataset, cache size and algorithm
from the name (`holes_<dataset>_cache<size>_<algo>.txt`) and scrapes the pairs
with a regex. Point it at the files and it works unmodified:

```bash
python analyze_holes.py holes_logs/*.txt
```

The dump is **off unless `--dump-holes` is given** — no flag, no file, and the
only cost on a normal run is one null check per request. If the flag is given, every
output path is probed **before the trace is read**, and an unwritable one
(usually a missing directory) stops the run in milliseconds rather than after a
multi-GB load. The directory is not created for you.

Three properties to preserve if you ever touch the writer:

- **Every served request gets a line, even with no holes.** `analyze_holes.py`
  counts holes per request per line, so the zero-hole lines are what keep that
  distribution honest. Dropping them biases it upward.
- **Skipped requests are omitted**, consistent with every other statistic here.
- **Holes are read from the phase-1 match**, not from the replay, so they are
  exactly the misses the reported hit ratio is computed from and the dump cannot
  perturb the simulation. Summing all hole lengths reproduces the miss ratio:
  11,283,747 of 15,470,907 blocks on the run above, i.e. 0.7294 against a
  reported hit ratio of 0.2706.

What it is good for, on that same run:

| | LRU | partial-node RandomCompute |
|---|---|---|
| avg holes / request | 1.00 | 1.02 |
| max holes / request | 1 | 3 |
| avg hole length | 262.9 | 253.0 |

LRU produces almost exactly one hole per request, so its resident set stays
prefix-contiguous — the deepest-first replay working as intended. The
partial-node policy occasionally punches a gap mid-prefix instead. That is the
contiguity difference that the removed `prefix_hit_ratio` metric used to
summarise in one number, now visible in full rather than averaged away.

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

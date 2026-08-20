# The `lcsllm` trace format

`lcsllm` is libCacheSim's binary trace format for **LLM prefix-cache traces**. It
is request-oriented: one record per inference request, carrying that request's
ordered list of KV blocks. That structure is the point — a prefix cache's
behaviour depends on which blocks belong to the same request, and a format that
emits one block at a time throws it away.

It replaces the earlier block-only `lcsllm` layout, which had no request
boundaries and no header. Nothing outside this repository reads that layout.

## At a glance

```
┌───────────────────────────────┐
│ file header (8192 B)          │  read once
├───────────────────────────────┤
│ request header (48 B)         │ ┐
│ block entry (16 B) × n_blocks │ ┘ one request
├───────────────────────────────┤
│ request header (48 B)         │
│ block entry (16 B) × n_blocks │
├───────────────────────────────┤
│ ...                           │
└───────────────────────────────┘
```

Everything is little-endian, packed, and naturally aligned. A reader never looks
ahead: read 48 bytes, learn `n_blocks`, read `n_blocks × 16` bytes, emit the
request, repeat. No offset tables, no seeking, no second pass. The whole format
streams, and works unchanged through zstd.

## File header — 8192 bytes

Same shape as the `lcs` header, with a **different magic**, so pointing a reader
at the wrong trace type fails loudly instead of misparsing.

| field | type | meaning |
|---|---|---|
| `start_magic` | u64 | `0x6c63736c6c6d0001`, validated on open |
| `version` | u64 | format version, currently 1 |
| `n_requests` | i64 | requests in the file |
| `n_blocks` | i64 | block entries in the file, summed over requests |
| `n_unique_blocks` | i64 | distinct block ids |
| `max_blocks_per_request` | i64 | lets a reader size its buffer once |
| `start_time_us`, `end_time_us` | i64 | first and last request timestamp |
| `block_size_tokens` | i32 | tokens per KV block, e.g. 16 |
| `block_id_kind` | u8 | always `1` = positional encoding — see below |
| `has_next_access` | u8 | whether `next_access_vtime` was computed |
| `n_types` | u8 | entries used in `type_names` |
| `type_names` | char[8][16] | request-type strings, indexed by `type_id` |
| `unused` | … | reserved, zeroed |
| `end_magic` | u64 | same value as `start_magic` |

## Positional encoding of block ids

**Block ids in an lcsllm file are always positionally encoded**, meaning:

> two blocks carry the same id only when the block *and its entire prefix* are
> identical.

The id is a hash folded over the whole path from the root up to and including
that block. This is a guarantee of the format, not a property a consumer has to
discover — `block_id_kind` is always `1`, a reader must refuse anything else, and
`traceConvLLM` has no flag to turn it off.

It is not a stylistic choice. A prefix cache is keyed on exactly this identity: a
KV block is reusable only if every block before it in the prompt matches, because
its contents were computed by attending over that whole prefix. Raw per-block
hashes cannot express it — in the qwen traces 16,686 raw ids appear under more
than one predecessor, so a simulator keyed on raw ids merges unrelated prefixes
into one cache object, inflating the hit ratio. Under a position-dependent cost
model it is worse than inflation: the same object arrives with two different
costs, which trips the cost-change check in `cache_find_base()`.

The repair has to happen at conversion time. Once ids are written, the
predecessor of each block is gone, so no consumer can recover which path an id
came from — this is why the format enforces the property instead of describing it.

Encoding a trace whose ids are *already* prefix-unique is safe: folding is then a
bijective relabeling, and it leaves cache behaviour untouched. Measured on
`prod_trace_w4_multiturn_concurrency_1024` (50k requests, LRU at 120k blocks),
raw and folded ids both give a block hit ratio of 0.178042 and compute savings of
0.090094 — identical to six digits.

`traceConvLLM` verifies the property on the way out rather than trusting it. The
encoding is deterministic, so the only way to violate it is a 64-bit collision
between two distinct paths; the converter checks that every id has a single
predecessor (which proves full path uniqueness inductively) and refuses to write
the file otherwise.

## Request header — 48 bytes

| off | field | type | meaning |
|---|---|---|---|
| 0 | `chat_id` | u64 | conversation this request belongs to |
| 8 | `parent_chat_id` | i64 | parent conversation, `-1` if none |
| 16 | `timestamp_us` | i64 | arrival time in **microseconds** |
| 24 | `n_blocks` | u32 | block entries that follow |
| 28 | `input_length` | u32 | prompt length in tokens |
| 32 | `output_length` | u32 | generated length in tokens |
| 36 | `turn` | u32 | turn index within the conversation |
| 40 | `type_id` | u8 | index into the header's `type_names` |
| 41 | `reserved` | u8[7] | zeroed; room to grow without a version bump |

48 is a multiple of 8, so the block array that follows is naturally aligned and
can be cast in place rather than copied field by field.

## Block entry — 16 bytes

| off | field | type | meaning |
|---|---|---|---|
| 0 | `block_id` | u64 | cache object identity |
| 8 | `next_access_vtime` | i64 | global block index of the next access, `-1` = never |

Blocks appear in **prefix order**: index 0 is the first block of the prompt (the
root of the shared path), the last index is the deepest, most private block.

`next_access_vtime` is a 1-based index into the flattened block sequence of the
whole trace, counted in **forward** request order, which is what `cache->n_req`
counts and therefore what `Belady` and `BeladyCompute` compare against. A reader
maps `-1` to `MAX_REUSE_DISTANCE`. When `has_next_access` is 0 the field is
`-1` everywhere and oracle algorithms cannot be run on the file.

## What is deliberately absent

**Cost.** Compute cost is a property of the *model*, not the trace — the same
block sequence costs differently on a 30B coder model than on a 7B one, and a
linear position proxy is not the same thing as a measured prefill profile.
Storing it in the trace is how `evaluate.cpp` (`863 + 2·i`) and the old
`.lcsllm` (`i + 1`) ended up disagreeing by two orders of magnitude in dynamic
range while both claiming to measure compute savings. Cost is therefore derived
at simulation time; `prefixsim --cost-model` selects the model.

**Object size.** Every KV block is one unit. Cache sizes are expressed in blocks.

## Two ways to read one file

*Request-grouped* — `prefixsim` reads whole requests and passes each block path
to `cache->record_request()`, which is what prefix-tree algorithms need.

*Flat* — `lcsllm_read_one_req()` walks the same file and emits one `request_t`
per block, ignoring boundaries, so `cachesim`, `traceAnalyzer` and `mrcProfiler`
work on `lcsllm` traces unchanged. It uses `reader->n_req_left` to track how many
blocks of the current request remain, the same mechanism block traces use when
splitting a large request.

## Size

For `qwen_coder_blksz_16_pos` (43,011 requests, 15,470,907 blocks):

| | |
|---|---|
| request headers | 2.0 MiB |
| block entries | 236.1 MiB |
| **total** | **238 MiB** (126 MiB as JSONL) |

The format is deliberately *larger* than the JSONL it replaces. The `_pos` traces
store small integers, so JSON already averages about 8 bytes per block against
this format's 16. What is bought is parse cost and structure: zero-copy casting
instead of `strtoull` per block, and request boundaries that no amount of parsing
can recover from the flat form. The layout compresses well — dense ids and
mostly-monotone next-access values — and libCacheSim reads zstd binary traces
transparently, so on-disk size is recoverable when it matters.

## Producing a trace

`traceConvLLM` converts a qwen-style JSONL trace:

```bash
./bin/traceConvLLM --input qwen_coder_blksz_16_pos.jsonl \
                   --output qwen_coder.lcsllm \
                   --block-size-tokens 16
```

See [Trace Utilities](quickstart_traceUtils.md) for the other converters.

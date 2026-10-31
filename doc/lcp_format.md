# The `lcp` trace format

`lcp` ("libCacheSim prefix") is libCacheSim's binary trace format for LLM
prefix-cache traces. One record per inference request, followed by that
request's ordered KV blocks.
Prefix-cache behaviour depends on which blocks belong to the same request, which
a block-at-a-time format cannot express.

The design follows [`lcs`](../libCacheSim/traceReader/customizedReader/lcs.h),
whose specification is that header file: a fixed 8 KiB header holding magic,
version and a stat block, laid out exactly as `lcs`'s, and one record struct per
version.

## Layout

```
┌──────────────────────────────────┐
│ header (8192 B)                  │  read once
├──────────────────────────────────┤
│ request record (68 B)            │ ┐
│ block entry (16 B) × n_blocks    │ ┘ one request
├──────────────────────────────────┤
│ request record                   │
│ block entry × n                  │
├──────────────────────────────────┤
│ ...                              │
└──────────────────────────────────┘
```

Little-endian, packed. A reader never looks ahead: read a record, learn
`n_blocks`, read that many entries, emit, repeat. No offset table, no seeking,
so the format streams, and works unchanged through zstd; files are normally
distributed compressed (`.lcp.zst`). Every piece of future knowledge a
simulator needs is stored, so one forward pass is a complete simulation (see
[Next access](#next-access)).

## Header — 8192 bytes

| field | type | meaning |
|---|---|---|
| `start_magic` | u64 | `0x6c63700000000001` |
| `version` | u64 | request record layout, currently 1 |
| `stat` | 8000 B | see below |
| `unused` | u64[21] | zeroed |
| `end_magic` | u64 | same as `start_magic` |

### Stat

| field | type | meaning |
|---|---|---|
| `version` | i64 | stat version, currently 1 |
| `n_req` | i64 | requests |
| `n_block` | i64 | block entries, summed over requests |
| `n_unique_block` | i64 | distinct block ids; ids are exactly `1..n_unique_block` |
| `n_session` | i64 | distinct session ids; ids are exactly `1..n_session` |
| `n_user` | i64 | distinct user ids; ids are exactly `1..n_user` |
| `max_blocks_per_req` | i64 | sizes a reader's buffer once |
| `first_complete_time_us`, `last_complete_time_us` | i64 | `complete_time_us` of the first and last request |
| `block_size_tokens` | i64 | tokens per KV block, e.g. 16 |
| `n_type` | i64 | entries used in `type_names` |
| `type_names` | char[32][32] | request-type strings, NUL-terminated, indexed by `type_id` |
| `unused` | i64[861] | zeroed |

## Request record — v1, 68 bytes

| off | field | type | meaning |
|---|---|---|---|
| 0 | `start_time_us` | i64 | arrival time, microseconds, at most `complete_time_us` (see [Time](#time)) |
| 8 | `complete_time_us` | i64 | completion time, microseconds |
| 16 | `chat_id` | i64 | source trace's `chat_id` |
| 24 | `parent_chat_id` | i64 | `chat_id` this request follows up |
| 32 | `session_next_req` | i64 | 1-based index of the next request with this `session_id`, `-1` = none |
| 40 | `session_id` | u32 | `1..n_session`; 0 = trace has no sessions |
| 44 | `user_id` | u32 | `1..n_user`; 0 = trace has no users |
| 48 | `n_blocks` | u32 | block entries that follow |
| 52 | `input_length` | i32 | prompt tokens |
| 56 | `output_length` | i32 | generated tokens |
| 60 | `turn` | i32 | turn index within the session |
| 64 | `type_id` | i32 | index into `type_names` |

`-1` in any other signed field means the source trace did not carry it (absent
or `null`). Requests appear in non-decreasing `complete_time_us` order.

### Time

Each request has two times, relative to the source trace's origin (not Unix
time) and kept to the microsecond:

- `start_time_us`: when the request arrived. Never after `complete_time_us`.
- `complete_time_us`: when the request completed, which is when its blocks are
  registered in the global cache and become reusable by later requests.
  Requests are ordered by it, so a follow-up turn always comes after the
  request it extends.

The converter reads them from the source's `timestamp_arrival` and
`timestamp`. fi-new and chutes record both. The Qwen traces record only
`timestamp`, which there is the arrival time, so a file converted from them
has the arrival time in both fields. In general, a source without arrival
times gets `start_time_us` equal to `complete_time_us`.

Arrival times in fi-new are computed upstream as completion minus latency and
carry float noise (`14701119.109104998` for `14701119.109105`); rounding to the
microsecond removes it.

## Block entry — 16 bytes

| off | field | type | meaning |
|---|---|---|---|
| 0 | `block_id` | u64 | cache object identity |
| 8 | `next_access_delta` | i64 | the block's next access, as the difference from the previous entry's (see [Stored as differences](#stored-as-differences)) |

Blocks appear in prefix order: index 0 is the first block of the prompt, the
last is the deepest.

## Next access

A block's next access and a request's `session_next_req` are 1-based positions
with `-1` for never, as in `lcs`.

A block's next access counts block accesses over the whole trace in forward
order, which is what `cache->n_req` counts and so what `Belady` and
`BeladyCompute` compare against (as `next_access_vtime`). `session_next_req`
counts requests in file order; it is what `session_belady` evicts by.

Both are the future, which a forward reader cannot know: deriving them takes a
reverse pass over the whole trace, and so the whole trace in memory (about 6 GB
for 200k requests). Storing them is what lets a simulator stream the file in one
pass, holding one request at a time. They are mandatory: every `lcp` file
carries them.

The request category of arXiv:2506.02634 (request type, turn) is not stored.
Readers intern it on the fly from `type_id` and `turn`, in order of first
appearance, which needs no lookahead.

### Stored as differences

A block entry stores its next access as `next_access_delta`, the difference
from the previous entry's next access in the same request. The first entry's
is the difference from 0, the value itself. A reader recovers the values with
a running sum that restarts at every request:

```
next = 0
for each block entry of the request:
    next += next_access_delta    # the block's next access
```

A follow-up request reuses its parent's prefix block by block, so consecutive
blocks are next accessed at consecutive positions, and a run of blocks that are
never accessed again is `-1` throughout. Over 99% of the differences are 1 or
0 in every trace in the [Size](#size) table. Stored as values, every next
access is a distinct 8-byte number that zstd cannot shrink, and the `.lcp.zst`
files there would be 3.2 (qwen_coder) to 18.8 (fi-new: 1.33 GiB instead of
72.5 MiB) times larger.

## Identifiers

**Block ids are positionally encoded and dense.** Two entries share an id iff
the block *and its entire prefix* are identical, and ids are assigned 1, 2,
3, ... in order of first appearance.

A prefix cache keys on exactly this identity: a KV block is reusable only if
every block before it matches, because its contents were computed by attending
over that prefix. Raw per-block hashes cannot express it — in the qwen traces
16,686 raw ids appear under more than one predecessor, so a simulator keyed on
them merges unrelated prefixes and inflates the hit ratio.

The converter assigns ids by exact interning: each `(parent id, raw id)` pair
gets the next integer, with parent 0 at the root. Exact by construction, so no
collision is possible. An earlier hash fold (`boost::hash_combine`) collided on
four of five traces checked, at 10^5–10^7 times the rate of a uniform 64-bit
hash: it barely mixes its inputs, and the `_pos` traces feed it sequential
integers. Dense ids also compress to less than half the size of hashed ones.

Where the source ids are already positionally encoded and dense in
first-appearance order, as in the qwen `_pos` traces, interning reproduces them
exactly. Encoding a trace whose ids are prefix-unique is a bijective relabeling
and leaves cache behaviour untouched.

Density is checkable in O(1) memory: each new id must be one past the largest
so far. The reader checks it on every block. It does not check that an id always
sits under the same parent, which would take memory proportional to
`n_unique_block`; positional encoding is guaranteed by the converter, not
verified by the reader. A consumer may
size per-block arrays by `n_unique_block`.

**Session ids** and **user ids** are the source's `session_id` and `user_id`
strings, interned the same way.
**Type ids** index the header's `type_names`. **Chat ids** are the source's
integers, carried verbatim.

## Example

Three requests from two sessions of two users, in the source JSONL (16-token
blocks; `hash_ids` are raw per-block hashes):

```json
{"chat_id":10,"parent_chat_id":-1,"timestamp":1.5,"timestamp_arrival":1.25,"user_id":"alice","input_length":40,"output_length":7,"type":"text","turn":0,"session_id":"a","hash_ids":[101,102,103]}
{"chat_id":11,"parent_chat_id":-1,"timestamp":2.0,"timestamp_arrival":1.5,"user_id":"bob","input_length":20,"output_length":5,"type":"text","turn":0,"session_id":"b","hash_ids":[101,103]}
{"chat_id":12,"parent_chat_id":10,"timestamp":3.25,"timestamp_arrival":2.75,"user_id":"alice","input_length":60,"output_length":9,"type":"text","turn":1,"session_id":"a","hash_ids":[101,102,103,104]}
```

convert to an 8,540-byte file before compression (8192 + 3 × 68 + 9 × 16):

```
header     n_req 3, n_block 9, n_unique_block 5, n_session 2, n_user 2,
           max_blocks_per_req 4, first_complete_time_us 1500000,
           last_complete_time_us 3250000, block_size_tokens 16,
           type_names ["text"]

request 1  start_time_us 1250000, complete_time_us 1500000, chat_id 10,
           parent_chat_id -1, session_next_req 3, session_id 1, user_id 1,
           n_blocks 3, input_length 40, output_length 7, turn 0, type_id 0
             block_id            1   2   3
             next_access_delta   4   3   1
             (next access        4   7   8)
request 2  start_time_us 1500000, complete_time_us 2000000, chat_id 11,
           parent_chat_id -1, session_next_req -1, session_id 2, user_id 2,
           n_blocks 2, input_length 20, output_length 5, turn 0, type_id 0
             block_id            1   4
             next_access_delta   6  -7
             (next access        6  -1)
request 3  start_time_us 2750000, complete_time_us 3250000, chat_id 12,
           parent_chat_id 10, session_next_req -1, session_id 1, user_id 1,
           n_blocks 4, input_length 60, output_length 9, turn 1, type_id 0
             block_id            1   2   3   5
             next_access_delta  -1   0   0   0
             (next access       -1  -1  -1  -1)
```

The rows in parentheses are not stored; they are the running sums a reader
computes.

- Raw `101` starts every prompt, so it is block 1 everywhere. Raw `103` appears
  under two prefixes — after `101, 102` in request 1, right after `101` in
  request 2 — so it becomes two blocks, 3 and 4. Request 3 extends request 1,
  so its first three blocks are 1, 2, 3 again.
- Block accesses are numbered 1–9 across the file: request 1 holds 1–3,
  request 2 holds 4–5, request 3 holds 6–9. Block 1 of request 1 is next
  accessed at 4, the first block of request 2; block 2 at 7, inside request 3,
  so its stored difference is 7 − 4 = 3.
- Session `a` becomes 1 and `b` becomes 2; user `alice` becomes 1 and `bob` 2.
  Request 1's `session_next_req` is 3: session `a` next appears in request 3.
- Requests are stored in completion order: request 2 completed at 2.0 s, after
  request 1 (1.5 s) and before request 3 (3.25 s).

## Versioning

As in `lcs`, `version` fixes the request record layout. A later version embeds
the v1 record as `base` and appends fields, so every version starts with v1 and
a reader can always read the common prefix. `stat.version` evolves separately; a
field added to the stat in a later version reads 0 in older traces, so 0 is
never a meaningful value for a new stat field.

## Reading

Two entry points in `lcp.c`, sharing one parser:

- **Per block**, `lcp_read_one_req()`: one `request_t` per block, ignoring
  request boundaries, so `cachesim`, `traceAnalyzer` and `mrcProfiler` read
  `lcp` unchanged. Per block it sets `obj_id`, `obj_size = 1`, `obj_cost`
  from the cost model chosen with `-t cost-model=<m>` (`llmCostModel.h`),
  `clock_time` in seconds (libCacheSim's convention), and `next_access_vtime`.
- **Per request**, `lcp_read_trace_req()`: a record, its block ids, and their
  next accesses. `prefixsim` streams with it (`--trace-format lcp`, or any file
  name containing `.lcp`), one pass per algorithm, one request in memory.

Both return next accesses as values, decoded from the stored differences.

Both stream, so memory does not grow with the trace. A `.zst` file is
decompressed through fixed buffers, plus the window zstd keeps of recent
output: 128 MiB for a file written with long-distance matching, as
`traceConvLLM` writes it. A plain file is memory-mapped, and the reader hands
pages back to the kernel once it has read past them, so it holds a window of
about 64 MiB rather than the whole file.

The reader refuses a file whose length disagrees with the header (truncated or
corrupt), a zstd stream that ends early, a wrong magic, an unknown version, a
block id that breaks density, a request whose `n_blocks`, `session_id`,
`user_id` or `type_id` exceeds what the header declares, a request that starts
after it completes, and a next access that decodes to a position that is not
after the block's own or is past the end of the trace. `tracePrint --print-stat 1` prints
the stat block.

`prefixsim` on an `lcp` file gives the same results as on its source JSONL,
which it loads whole and annotates in memory: both readers intern ids and
categories with the same code.

## Size

Compressed as `traceConvLLM` writes it, equivalent to `zstd -8 --long`, and
the source JSONL compressed the same way for comparison:

| trace | requests | block accesses | JSONL | JSONL, zstd | `lcp` | `lcp.zst` |
|---|---|---|---|---|---|---|
| `qwen_coder_blksz_16_pos` | 43,011 | 15.5M | 125.9 MiB | 4.4 MiB | 238.9 MiB | 10.3 MiB |
| fi-new (`_max256k_pos`) | 327,548 | 656M | 6.79 GiB | 60.0 MiB | 9.80 GiB | 72.5 MiB |
| chutes (`_max256k_pos`) | 515,817 | 604M | 6.74 GiB | 146.0 MiB | 9.03 GiB | 221.5 MiB |

A compressed `lcp` file is 1.2 to 2.3 times the size of the compressed JSONL,
and it carries the next-access times the JSONL lacks.

## Producing a trace

```bash
./bin/traceConvLLM --input qwen_coder_blksz_16_pos.jsonl \
                   --output qwen_coder.lcp \
                   --block-size-tokens 16
# wrote qwen_coder.lcp.zst: 43011 requests, ...
```

The output is zstd-compressed by default, at level 8 with long-distance
matching (128 MiB window, frame checksum), and `.zst` is appended to the name
when missing, since readers recognise zstd by it. `--no-zstd` writes a plain
file.

The converter reuses `prefixsim`'s JSONL reader, so it reads the fields
`hash_ids`, `timestamp` and `timestamp_arrival` (seconds, numeric), `chat_id`,
`parent_chat_id`, `session_id`, `user_id`, `input_length`, `output_length`,
`turn` and `type`. It refuses a trace whose timestamps decrease, are negative,
or all parse as 0, one with a request that arrives after it completes, and
one with more than 32 request types. It holds the whole
trace in memory: 12.8 GiB and about 2 minutes for fi-new, 15.4 GiB and about
3 minutes for chutes.

See [Trace Utilities](quickstart_traceUtils.md) for the other converters.

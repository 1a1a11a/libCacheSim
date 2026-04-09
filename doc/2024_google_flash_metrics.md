# 2024 Google Trace Flash Metrics

This document records request miss ratio, byte miss ratio, and flash write
ratio for FIFO, CLOCK, and LRU on the three traces under `2024_google/`.

## Setup

- Traces:
  - `2024_google/cluster1_16TB.lcs.zst`
  - `2024_google/cluster2_16TB.lcs.zst`
  - `2024_google/cluster3_18TB.lcs.zst`
- Trace type: `lcs`
- Cache fractions: `0.001`, `0.01`
- Algorithms: `fifo`, `clock`, `lru`
- Runner: `_build_dbg/bin/flashMetrics`

## Flash Write Definitions

- FIFO: `flash writes = byte miss ratio`
- CLOCK: `flash writes = byte miss ratio + byte reinsertion ratio`
- LRU: `flash writes = byte miss ratio + byte promotion ratio`

## Commands

```bash
export PATH="/users/juncheng/software/cmake/bin:$PATH"
bash scripts/debug.sh -c

_build_dbg/bin/flashMetrics 2024_google/cluster1_16TB.lcs.zst lcs fifo,clock,lru 0.001,0.01 --num-thread=2 --print-head-req=false --verbose=false
_build_dbg/bin/flashMetrics 2024_google/cluster2_16TB.lcs.zst lcs fifo,clock,lru 0.001,0.01 --num-thread=2 --print-head-req=false --verbose=false
_build_dbg/bin/flashMetrics 2024_google/cluster3_18TB.lcs.zst lcs fifo,clock,lru 0.001,0.01 --num-thread=2 --print-head-req=false --verbose=false
```

## Resolved Cache Sizes

- `cluster1_16TB.lcs.zst`
  - `0.001` -> `16594555823` bytes
  - `0.01` -> `165945558237` bytes
- `cluster2_16TB.lcs.zst`
  - `0.001` -> `17936923773` bytes
  - `0.01` -> `179369237734` bytes
- `cluster3_18TB.lcs.zst`
  - `0.001` -> `20588812227` bytes
  - `0.01` -> `205888122275` bytes

## Results

| Trace | Cache frac | Algo | Cache size (bytes) | Req miss ratio | Byte miss ratio | Extra ratio | Flash writes |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| cluster1_16TB | 0.001 | FIFO | 16594555823 | 0.07250300 | 0.10009658 | none = 0.00000000 | 0.10009658 |
| cluster1_16TB | 0.010 | FIFO | 165945558237 | 0.04770304 | 0.07727100 | none = 0.00000000 | 0.07727100 |
| cluster1_16TB | 0.001 | CLOCK | 16594555823 | 0.07092634 | 0.09869641 | reinsertion = 0.08800090 | 0.18669730 |
| cluster1_16TB | 0.010 | CLOCK | 165945558237 | 0.04411648 | 0.07548000 | reinsertion = 0.07950309 | 0.15498310 |
| cluster1_16TB | 0.001 | LRU | 16594555823 | 0.07083643 | 0.09982770 | promotion = 0.80526211 | 0.90508981 |
| cluster1_16TB | 0.010 | LRU | 165945558237 | 0.04360217 | 0.07543045 | promotion = 1.03627784 | 1.11170829 |
| cluster2_16TB | 0.001 | FIFO | 17936923773 | 0.08057969 | 0.10995881 | none = 0.00000000 | 0.10995881 |
| cluster2_16TB | 0.010 | FIFO | 179369237734 | 0.05957426 | 0.08904639 | none = 0.00000000 | 0.08904639 |
| cluster2_16TB | 0.001 | CLOCK | 17936923773 | 0.07892277 | 0.10866495 | reinsertion = 0.09047547 | 0.19914042 |
| cluster2_16TB | 0.010 | CLOCK | 179369237734 | 0.05604016 | 0.08592051 | reinsertion = 0.07462376 | 0.16054427 |
| cluster2_16TB | 0.001 | LRU | 17936923773 | 0.07909499 | 0.10916144 | promotion = 0.60058168 | 0.70974311 |
| cluster2_16TB | 0.010 | LRU | 179369237734 | 0.05565730 | 0.08578171 | promotion = 0.64529025 | 0.73107196 |
| cluster3_18TB | 0.001 | FIFO | 20588812227 | 0.05021450 | 0.09560787 | none = 0.00000000 | 0.09560787 |
| cluster3_18TB | 0.010 | FIFO | 205888122275 | 0.03976046 | 0.07676905 | none = 0.00000000 | 0.07676905 |
| cluster3_18TB | 0.001 | CLOCK | 20588812227 | 0.04947112 | 0.09444140 | reinsertion = 0.08930696 | 0.18374837 |
| cluster3_18TB | 0.010 | CLOCK | 205888122275 | 0.03857983 | 0.07492515 | reinsertion = 0.07284486 | 0.14777001 |
| cluster3_18TB | 0.001 | LRU | 20588812227 | 0.04920643 | 0.09435400 | promotion = 0.80822255 | 0.90257655 |
| cluster3_18TB | 0.010 | LRU | 205888122275 | 0.03830963 | 0.07459867 | promotion = 0.90057613 | 0.97517480 |

## Notes

- The LRU flash write ratio can exceed `1.0` because the promotion term is
  normalized by requested bytes and hot bytes can be promoted multiple times.
- The CLOCK extra term is the measured byte reinsertion ratio from the cache
  implementation.
- The FIFO extra term is zero by definition for this report.

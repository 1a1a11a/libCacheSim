# Native synthetic traces

Use the `syn` trace type to generate requests directly in the native reader:

```sh
./_build/bin/cachesim Uniform_1000000_10000 syn lru 1000
./_build/bin/cachesim Zipf_1000000_10000_0.9_42 syn lru,s3fifo 100,1000
```

The specifications are `Uniform_n_m[_seed]` and `Zipf_n_m_alpha[_seed]`:

| Parameter | Meaning | Supported values |
| --- | --- | --- |
| `n` | Number of source requests, before sampling or a request limit | Integer from 1 through `INT64_MAX` |
| `m` | Number of possible objects | Integer from 1 through `INT32_MAX` |
| `alpha` | Zipf exponent; zero selects uniform generation | Finite nonnegative decimal number, including scientific notation |
| `seed` | Independent workload seed | Integer from 0 through `UINT64_MAX`; defaults to 1 |

Distribution names are case-insensitive. Numeric fields must be complete tokens;
negative values, overflow, exponents that underflow to zero, nonfinite exponents
and trailing text are rejected.

IDs are in `[0, m)`. Uniform requests select each ID with probability `1/m`.
For Zipf, ID `k` has probability proportional to `(k + 1)^(-alpha)` over that
finite range. Requests are sampled with replacement; a finite trace need not
contain every object.

Each request has size and cost 1, operation `OP_NOP`, no TTL, and timestamp 0,
matching a plain-text trace without timestamps. There is no arrival-time model;
time-based warmup and analysis are not meaningful for this source. Future-access
metadata is unavailable, so it cannot directly drive offline Belady algorithms.
Reader options for columns, file offsets, headers, block splitting and string
object IDs do not apply. Ordinary spatial sampling and request limits apply to
the generated stream. A positive request limit counts source reads, including
requests discarded by sampling; zero and negative limits mean unlimited.

Fractional/automatic cache capacities retain the existing working-set scan.
That scan consumes time and memory proportional to the observed workload; use
absolute capacities when avoiding that preprocessing is important. Generation
itself keeps constant-size state, without a probability table or trace buffer.

## Library use and reproducibility

```c
reader_t *reader = setup_reader("Zipf_1000000_10000_0.9_42",
                                SYNTHETIC_TRACE, NULL);
request_t *req = new_request();
while (read_one_req(reader, req) == 0) {
  /* Consume req. */
}
free_request(req);
close_reader(reader);
```

Reset replays the same sequence. A clone starts at the beginning with the same
configuration and independent state, following the existing reader convention.
The source cursor supports skipping, fractional seeking and reverse traversal
without replaying the prefix. First/last-request helpers leave the original
reader untouched. Counting requests returns the declared source length.

The generator uses Philox2x64-10 with the source index and rejection-draw index
as its counter and the seed as its key. It never consumes the eviction
algorithms' random state. Changing worker count or algorithm order does not
change the workload, and increasing `n` preserves its prefix. Randomized cache
algorithms may still produce different simulation results.

Uniform sampling removes modulo bias. Bounded Zipf uses rejection-inversion,
with stable integral/inverse calculations around exponent 1, adapted from
Apache Commons RNG. The object-count bound keeps the ranks within the reference
algorithm's supported integer range. Floating-point precision limits very small
tail probabilities. Zipf output is reproducible within the same implementation
and numerical environment; identical output across different math libraries or
future generator changes is not guaranteed. Materialize a trace when exact
long-term replay across environments is required.

Spatial sampling preserves source-index navigation. Stateful temporal sampling
supports forward reads, reset and clones; seek/skip/reverse move the source
cursor, not the temporal sampler's history. Reset before a new forward pass.

## Validation

`testSyntheticReader` covers published RNG vectors, reset/clone/navigation,
sampling and limits, finite-distribution checks, and equality with a materialized
trace for LRU, FIFO and S3FIFO in serial and parallel simulations. `testCLI`
covers synthetic syntax and invalid inputs.

```sh
ctest --test-dir _build --output-on-failure
```

Reader throughput includes generation cost. For benchmarks, report generation
and simulation separately; replaying an existing binary file can be faster when
many experiments reuse a workload.

# Synthetic traces

Sometimes you want a workload with known properties, such as "a million requests over ten thousand objects, Zipf-distributed". You may also have no trace file at hand. The `syn` trace type generates requests on the fly. Instead of a file path, you pass a short description of the workload.

All commands run from the build directory, `_build/`.

## Quick start

```bash
# 1,000,000 requests spread evenly over 10,000 objects
./bin/cachesim Uniform_1000000_10000 syn lru 1000

# 1,000,000 requests over 10,000 objects, Zipf with alpha 0.9, seed 42
./bin/cachesim Zipf_1000000_10000_0.9_42 syn lru,s3fifo 100,1000
```

```text
Zipf_1000000_10000_0.9_42 LRU cache size      100B, 1000000 req, miss ratio 0.7392, byte miss ratio 0.7392
Zipf_1000000_10000_0.9_42 LRU cache size     1000B, 1000000 req, miss ratio 0.4447, byte miss ratio 0.4447
Zipf_1000000_10000_0.9_42 S3FIFO-0.1000-2 cache size      100B, 1000000 req, miss ratio 0.6148, byte miss ratio 0.6148
Zipf_1000000_10000_0.9_42 S3FIFO-0.1000-2 cache size     1000B, 1000000 req, miss ratio 0.3661, byte miss ratio 0.3661
```

Every object has size 1, so a cache size of `1000` holds 1000 objects.

## Describe the workload

There are two forms:

```text
Uniform_<n>_<m>[_<seed>]
Zipf_<n>_<m>_<alpha>[_<seed>]
```

| Field | Meaning | Allowed values |
| --- | --- | --- |
| `n` | Number of requests to generate, before any sampling or request limit | Whole number from 1 to `INT64_MAX` |
| `m` | Number of distinct objects | Whole number from 1 to `INT32_MAX` |
| `alpha` | Zipf skew. Larger means a few objects get more of the requests. `0` gives a uniform distribution. | Finite number ≥ 0; scientific notation such as `9e-1` is allowed |
| `seed` | Seed for the workload | Whole number from 0 to `UINT64_MAX`; defaults to 1 |

The distribution name is not case-sensitive. Each number must be the whole field. Negative values, overflow, an exponent that rounds down to zero, infinity or NaN, and trailing characters are all rejected.

### What the requests look like

* Object ids are `0` to `m - 1`.
* **Uniform**: each id is equally likely, with probability `1/m`.
* **Zipf**: id `k` has probability proportional to `(k + 1)^(-alpha)`. Id 0 is the most popular.
* Ids are drawn independently, with replacement, so a short trace may never touch some objects.
* Every request has size 1, cost 1, operation `OP_NOP`, no TTL, and timestamp 0. That matches a plain-text trace without timestamps.

## What does not work

* **Anything based on time.** There are no arrival times, so time-based warmup (`--warmup-sec`) and time-based analysis are meaningless.
* **Belady and other offline algorithms.** The generator does not know when an object will be requested next, so these cannot run directly on a `syn` trace. If you need them, write the workload to an oracle trace with [traceConv](quickstart_traceUtils.md) first:

  ```bash
  ./bin/traceConv Zipf_1000000_10000_0.9_42 syn --output-format=oracleGeneral -o zipf.oracleGeneral
  ./bin/cachesim zipf.oracleGeneral oracleGeneral belady 1000
  ```
* **File reader options.** Column settings, file offsets, headers, block splitting and string object ids do not apply.

What does work: spatial sampling (`-s`) and request limits (`--num-req`). The request limit counts generated requests, including ones that sampling then drops. A limit of zero or a negative number means no limit.

## Memory and speed

The generator keeps a small, fixed amount of state. It has no probability table and does not buffer the trace, so memory does not grow with `n` or `m`.

Fractional or `auto` cache sizes still need the working set size, so `cachesim` first scans the whole generated workload. That scan costs time and memory proportional to the workload. Give absolute cache sizes if you want to skip it.

Reader throughput includes the cost of generating each request. When benchmarking, report generation and simulation separately. If many experiments reuse one workload, replaying a binary trace file can be faster than regenerating it each time.

## Use it from the C library

```c
reader_t *reader = setup_reader("Zipf_1000000_10000_0.9_42",
                                SYNTHETIC_TRACE, NULL);
request_t *req = new_request();
while (read_one_req(reader, req) == 0) {
  /* use req */
}
free_request(req);
close_reader(reader);
```

The reader behaves like a file reader:

* **Reset** replays the same sequence from the start.
* **Clone** starts at the beginning with the same settings and its own independent state.
* **Skip, seek to a fraction, and read backwards** all work without replaying the earlier requests.
* The first-request and last-request helpers leave the original reader where it was.
* The request count is the `n` you asked for.

## Reproducibility

The same description gives the same workload.

* The generator is a counter-based random number generator, Philox2x64-10. Its counter is the request index (plus a rejection-draw index), and its key is the seed.
* It never touches the random state that eviction algorithms use. Changing the number of threads or the order of algorithms does not change the workload.
* Making `n` larger keeps the earlier requests the same. A longer trace starts with the shorter one.
* Randomized eviction algorithms can still give different results from run to run. That randomness lives in the algorithm, not the workload.

Some numerical details:

* Uniform sampling has no modulo bias.
* Bounded Zipf uses rejection-inversion, adapted from Apache Commons RNG, with numerically stable formulas near `alpha = 1`. The `m` limit keeps ranks inside the integer range that algorithm supports.
* Floating-point precision limits how well very small tail probabilities are represented.
* Zipf output is reproducible with the same code and numerical environment. A different math library, or a future change to the generator, may produce a different sequence. If you need the exact same requests years from now or on other machines, write the trace to a file with `traceConv` (as shown above) and keep that file.

### Sampling and navigation

Spatial sampling works with skip, seek and reverse reads.

Temporal sampling keeps history, so it only fully supports forward reads, reset and cloning. Skip, seek and reverse reads move the position in the generated stream, but they do not rewind the temporal sampler's history. Reset the reader before starting a new forward pass.

## Validation

Two tests cover the generator:

* `testSyntheticReader` checks the generator against published random number test vectors. It also covers reset, clone and navigation, sampling and request limits, and checks on the resulting distributions. Finally, it confirms that LRU, FIFO and S3FIFO give the same results on a synthetic trace as on the same trace written to a file, in serial and in parallel simulations.
* `testCLI` covers the command-line syntax and rejection of bad input.

To run the tests, go to the repository root and run:

```bash
ctest --test-dir _build --output-on-failure
```

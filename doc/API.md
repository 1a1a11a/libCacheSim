# C API reference

This page lists the parts of the C API you'll use most often, grouped by task. If you're new to the library, start with the [library guide](advanced_lib.md), which builds a working simulator step by step, and come back here to look things up.

Everything below comes from one header:

```c
#include <libCacheSim.h>
```

Compile with pkg-config:

```bash
gcc your_program.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o your_program -lm -lzstd
```

Add `-lstdc++` if you call `create_cache_by_name()` or construct a C++ algorithm. This is a curated subset. The headers under [libCacheSim/include/libCacheSim/](/libCacheSim/include/libCacheSim/) are the source of truth, and complete programs live in [the example folder](/example).

---

## Which function do I need?

| I want to | Use |
| --- | --- |
| Open a trace file | [`open_trace()`](#opening-a-trace) with a [`reader_init_param_t`](#reader-parameters) |
| Read the next request | [`read_one_req()`](#iterating-over-requests) into a [`request_t`](#requests) |
| Create a cache | [`<Algo>_init()` or `create_cache_by_name()`](#creating-a-cache) |
| Send a request to a cache | [`cache->get()`](#using-a-cache) |
| Control insert and evict myself | [`find`, `insert`, `evict`, `to_evict`, `remove`](#using-a-cache) |
| Add admission or prefetching | [`create_admissioner()`, `create_prefetcher()`](#admission-and-prefetching) |
| Sweep many sizes or caches in parallel | [`simulate_*()`](#simulator) |
| Compute reuse or stack distances | [`get_stack_dist()`, `get_access_dist()`](#trace-statistics) |

---

## Reading traces

### Opening a trace

```c
/* open a trace; release it with close_trace() */
static inline reader_t *open_trace(const char *path, trace_type_e type,
                                   const reader_init_param_t *reader_init_param);

/* the same function under its original name */
reader_t *setup_reader(const char *trace_path, trace_type_e trace_type,
                       const reader_init_param_t *reader_init_param);
```

You can pass `NULL` for the parameters when the format describes itself (vscsi, oracleGeneral, lcs, twr, synthetic). For csv, txt, and generic binary traces, pass parameters that say where each field is.

The trace types are listed in `trace_type_e` in [enum.h](/libCacheSim/include/libCacheSim/enum.h). The common ones are:

| Type | Format |
| --- | --- |
| `CSV_TRACE` | csv, with columns given in the parameters |
| `PLAIN_TXT_TRACE` | one object id per line |
| `BIN_TRACE` | fixed-size binary records described by `binary_fmt_str` |
| `ORACLE_GENERAL_TRACE` | binary records with time, id, size, and next-access position |
| `LCS_TRACE` | libCacheSim's own binary format |
| `VSCSI_TRACE` | VMware vscsi block traces |
| `TWR_TRACE`, `TWRNS_TRACE` | Twitter cache traces |
| `VALPIN_TRACE` | valpin binary traces |
| `LCP_TRACE` | LLM prefix-cache traces (see [PrefixSim](https://github.com/cacheMon/PrefixSim/blob/main/doc/lcp_format.md)) |
| `SYNTHETIC_TRACE` | generated requests; the "path" is a spec like `Zipf_1000000_10000_0.9_42` (see [synthetic traces](synthetic_traces.md)) |

### Reader parameters

`reader_init_param_t` describes how to parse a trace. Field indices start at 1, and `0` means the field is absent.

**Always start from `default_reader_init_params()`.** Several defaults are not zero (`cap_at_n_req = -1`, `block_size = -1`, `ignore_size_zero_req = true`, `delimiter = ','`), so a zero-initialized struct quietly changes behavior.

```c
typedef struct {
  bool ignore_obj_size;           /* treat every object as size 1 */
  bool ignore_size_zero_req;      /* skip zero-size requests; default true */
  bool obj_id_is_num;             /* default TRUE; see the warning below */
  bool obj_id_is_num_set;         /* set this too, or obj_id_is_num is ignored */
  int64_t cap_at_n_req;           /* read at most n requests; -1 = all */

  int32_t time_field;
  int32_t obj_id_field;
  int32_t obj_size_field;
  int32_t obj_cost_field;
  int32_t op_field;
  int32_t ttl_field;
  int32_t cnt_field;
  int32_t tenant_field;
  int32_t next_access_vtime_field;

  int32_t n_feature_fields;
  int32_t feature_fields[N_MAX_FEATURES];

  int32_t block_size;             /* block traces: split large requests into blocks */

  /* csv */
  bool has_header;
  bool has_header_set;            /* set this too, or the header is auto-detected */
  char delimiter;

  ssize_t trace_start_offset;     /* skip metadata at the start of a binary trace */
  char *binary_fmt_str;           /* binary: a Python struct format string, e.g. "<IIIHHQQ" */
  char *llm_cost_model;           /* lcp: how obj_cost is derived; NULL = uniform */

  sampler_t *sampler;             /* optional request sampling */
  bool require_valid_next_access; /* reject zero-size filtering that breaks oracle data */
  bool ignore_ttl;                /* drop every request's TTL */
} reader_init_param_t;

static inline reader_init_param_t default_reader_init_params(void);
```

> [!IMPORTANT]
> `default_reader_init_params()` sets `obj_id_is_num` to **true**, which is the opposite of what `cachesim` does. If the id column holds strings, set `obj_id_is_num = false` and `obj_id_is_num_set = true`. Otherwise the csv reader parses each id with `strtoull()`, which warns and returns `0`. Every object then collapses into one, and the miss ratio means nothing.

```c
reader_init_param_t p = default_reader_init_params();
p.obj_id_is_num = false;      /* string ids: hash them */
p.obj_id_is_num_set = true;
```

If you replay an oracle trace for Belady or BeladySize in your own loop, set `require_valid_next_access = true`. See [oracle traces](oracle_traces.md).

### Iterating over requests

```c
/* read the next request into req; returns 0 on success, 1 at end of trace */
int read_one_req(reader_t *reader, request_t *req);

/* number of requests in the trace */
int64_t get_num_of_req(reader_t *reader);

static inline trace_type_e get_trace_type(const reader_t *reader);
static inline bool obj_id_is_num(const reader_t *reader);

/* rewind so the trace can be read again */
void reset_reader(reader_t *reader);

/* independent copy; the usual way to give each thread its own reader */
reader_t *clone_reader(const reader_t *reader);

int close_reader(reader_t *reader);
static inline int close_trace(reader_t *reader);   /* same as close_reader */
```

Positioning helpers, used mostly by the analysis tools:

```c
void read_first_req(reader_t *reader, request_t *req);
void read_last_req(reader_t *reader, request_t *req);
int skip_n_req(reader_t *reader, int N);
int go_back_one_req(reader_t *reader);
void reader_set_read_pos(reader_t *reader, double pos);  /* pos in [0, 1] */
```

---

## Requests

A `request_t` is the container that `read_one_req()` fills in. Allocate one up front and reuse it for the whole trace.

```c
static inline request_t *new_request(void);
static inline void copy_request(request_t *req_dest, const request_t *req_src);
static inline request_t *clone_request(const request_t *req);
static inline void free_request(request_t *req);
static inline void print_request(const request_t *req);
```

The fields you'll usually read:

| Field | Meaning |
| --- | --- |
| `obj_id` | object id (hashed, if the trace's ids are strings) |
| `obj_size` | size in bytes |
| `clock_time` | timestamp from the trace (seconds in the sample traces) |
| `obj_cost` | cost of a miss; 1 unless the trace says otherwise |
| `op` | operation (get, set, delete, ...) when the trace records it |
| `ttl` | time to live, when the trace records it |
| `next_access_vtime` | position of this object's next request, in oracle traces only |

---

## Caches

### Creating a cache

Every eviction algorithm has an `_init` function. It takes the common parameters, plus an optional algorithm-specific parameter string, the same string `cachesim` takes with `-e` (for example `"n-seg=4"`). Pass `NULL` for the defaults.

```c
typedef struct {
  uint64_t cache_size;         /* bytes, or object count when sizes are ignored */
  uint64_t default_ttl;
  int32_t hashpower;           /* log2 of the initial hash table size */
  bool consider_obj_metadata;  /* count per-object metadata against cache_size */
  int64_t n_total_req;         /* trace length if known, 0 = unknown */
} common_cache_params_t;

/* 1 GiB, hashpower 20, metadata not counted */
common_cache_params_t default_common_cache_params(void);

cache_t *LRU_init(common_cache_params_t ccache_params,
                  const char *cache_specific_params);
/* ... and FIFO_init, ARC_init, S3FIFO_init, Sieve_init, and the rest;
 * see libCacheSim/include/libCacheSim/evictionAlgo.h */

/* look up a built-in algorithm by its cachesim name;
 * returns NULL for an unknown name */
cache_t *create_cache_by_name(const char *cache_algo_name,
                              const common_cache_params_t cc_params,
                              const char *cache_specific_params);
```

`cachesim` uses hashpower 24 by default, while `default_common_cache_params()` uses 20. The table grows on its own, so the value only affects memory and resize cost. `create_cache_by_name()` can build Belady and BeladySize, but those need an oracle trace, so check the trace type before you ask for them.

### Using a cache

You use a `cache_t` through its function pointers:

```c
/* the whole thing: lookup, plus insert and evict on a miss.
 * Returns true on a hit. Also advances cache->n_req. */
bool (*get)(cache_t *, const request_t *);

/* lookup only; update_cache controls whether the lookup also
 * updates state such as recency. Returns NULL on a miss. */
cache_obj_t *(*find)(cache_t *, const request_t *, bool update_cache);

/* whether the object can be inserted at all (e.g. it fits) */
bool (*can_insert)(cache_t *, const request_t *);

/* insert without evicting; make room first. Returns NULL if not inserted. */
cache_obj_t *(*insert)(cache_t *, const request_t *);

/* which object would be evicted next, without evicting it */
cache_obj_t *(*to_evict)(cache_t *, const request_t *);

/* evict one object */
void (*evict)(cache_t *, const request_t *);

/* user-triggered removal; eviction should go through evict */
bool (*remove)(cache_t *, obj_id_t);

int64_t (*get_occupied_byte)(const cache_t *);
int64_t (*get_n_obj)(const cache_t *);

void (*cache_free)(cache_t *);
```

Most programs only need `get()`. If you drive `find`, `insert`, and `evict` yourself, increment `cache->n_req` once per user request. Algorithms that measure time in requests depend on it.

Three more pointers are optional and `NULL` for most algorithms: `record_request`, `set_request_ctx`, and `evict_n`. They exist for algorithms that need whole-request information, such as LLM prefix caches. Check for `NULL` before calling them. The [library guide](advanced_lib.md#advanced-optional-hooks-for-request-aware-algorithms) explains when to call each. To implement a new algorithm, see [Adding algorithms and trace readers](advanced_lib_extend.md).

### Admission and prefetching

```c
/* names: bloomfilter (bloom-filter), prob (probabilistic), size,
 *        sizeProbabilistic (sizeProb), adaptsize */
static inline admissioner_t *create_admissioner(const char *admission_algo,
                                                const char *admission_params);

/* in <libCacheSim/prefetchAlgo.h>, which libCacheSim.h does not include.
 * names: Mithril, OBL, PG */
static inline prefetcher_t *create_prefetcher(const char *prefetching_algo,
                                              const char *prefetching_params,
                                              uint64_t cache_size);
```

Attach them by assigning `cache->admissioner` or `cache->prefetcher`.

---

## Simulator

Instead of writing the replay loop yourself, you can hand a trace and a cache to the simulator. It runs the simulations in parallel across a thread pool.

```c
/* one cache, many sizes; `cache` is a template the caller still owns */
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache,
                                      int num_of_sizes,
                                      const uint64_t *cache_sizes,
                                      reader_t *warmup_reader,
                                      double warmup_frac, int warmup_sec,
                                      int num_of_threads, bool use_random_seed);

/* one cache at step_size, 2*step_size, ... up to cache_in->cache_size */
cache_stat_t *simulate_at_multi_sizes_with_step_size(
    reader_t *reader_in, const cache_t *cache_in, uint64_t step_size,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool use_random_seed);

/* many caches, each with its own algorithm and size */
cache_stat_t *simulate_with_multi_caches(
    reader_t *reader, cache_t *caches[], int num_of_caches,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool free_cache_when_finish, bool use_random_seed);
```

To skip warmup, pass `NULL`, `0`, and `0` for `warmup_reader`, `warmup_frac`, and `warmup_sec`. Each call returns an array with one `cache_stat_t` per simulation, and the caller frees the array with `free()`:

```c
typedef struct {
  int64_t n_warmup_req;
  int64_t n_req;
  int64_t n_req_byte;
  double n_req_cost;
  int64_t n_miss;
  int64_t n_miss_byte;
  double n_miss_cost;

  int64_t n_obj;
  int64_t occupied_byte;
  int64_t cache_size;
  float sampler_ratio;
  int64_t curr_rtime;
  int64_t expired_obj_cnt;
  int64_t expired_bytes;

  char cache_name[CACHE_NAME_ARRAY_LEN];
} cache_stat_t;
```

The object miss ratio is `n_miss / n_req`, and the byte miss ratio is `n_miss_byte / n_req_byte`.

---

## Trace statistics

These live in `<libCacheSim/dist.h>`, which `libCacheSim.h` does not include.

```c
/* unique objects since the last access (STACK_DIST) or until the next
 * one (FUTURE_STACK_DIST); one entry per request, -1 if there is none */
int32_t *get_stack_dist(reader_t *reader, const dist_type_e dist_type,
                        int64_t *array_size);

/* requests since the last (DIST_SINCE_LAST_ACCESS) or first
 * (DIST_SINCE_FIRST_ACCESS) access; one entry per request */
int32_t *get_access_dist(reader_t *reader, const dist_type_e dist_type,
                         int64_t *array_size);
```

The caller frees the returned array.

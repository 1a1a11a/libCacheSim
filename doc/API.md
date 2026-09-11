# C API reference

The public C API is exposed through a single header:

```c
#include <libCacheSim.h>
```

Compile against it with pkg-config:

```bash
gcc your_program.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o your_program -lm -lzstd
```

See [advanced_lib.md](advanced_lib.md) for a walkthrough and [the example folder](/example) for complete programs. The declarations below are the commonly used subset; the headers under [libCacheSim/include/libCacheSim/](/libCacheSim/include/libCacheSim/) are authoritative.

---

## Reading traces

### Opening a trace

`reader_init_param_t` describes how to interpret a trace. Field indices are 1-based, and `0` means the field is absent. Start from `default_reader_init_params()` rather than zero-initializing, so the defaults for delimiter, sampling, and the "was this set by the user" flags are correct.

```c
typedef struct {
  bool ignore_obj_size;
  bool ignore_size_zero_req;
  bool obj_id_is_num;
  bool obj_id_is_num_set;   // whether the user passed this parameter
  int64_t cap_at_n_req;     // process at most n requests

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

  // block cache; breaks a large request into per-block requests
  int32_t block_size;

  // csv reader
  bool has_header;
  bool has_header_set;      // false alone cannot distinguish "unset"
  char delimiter;

  // skip metadata at the start of a binary trace
  ssize_t trace_start_offset;

  // binary reader, a Python struct format string
  char *binary_fmt_str;

  sampler_t *sampler;
} reader_init_param_t;

static inline reader_init_param_t default_reader_init_params(void);

/**
 * open a trace for reading; the reader must be released with close_trace()
 * @param trace_type CSV_TRACE, PLAIN_TXT_TRACE, BIN_TRACE, VSCSI_TRACE,
 *                   ORACLE_GENERAL_TRACE, TWR_BIN_TRACE, LCS_TRACE, ...
 */
reader_t *setup_reader(const char *trace_path, trace_type_e trace_type,
                       const reader_init_param_t *reader_init_param);

/* same function as setup_reader, and the more commonly used name */
static inline reader_t *open_trace(const char *path, trace_type_e type,
                                   const reader_init_param_t *reader_init_param);
```

> [!IMPORTANT]
> `default_reader_init_params()` sets `obj_id_is_num` to **true**, which is the opposite of what `cachesim` does. Set it to `false` yourself if the id field holds strings. The csv reader hashes string ids only on the `false` path; with `true` it runs them through `strtoull()`, which warns and yields `0`, so every object collapses into one and the miss ratio is meaningless.

```c
reader_init_param_t p = default_reader_init_params();
p.obj_id_is_num = false;   /* string ids: hash them */
```

### Iterating over requests

```c
/* read one request into the pre-allocated req; returns 0 on success,
 * 1 at end of trace */
int read_one_req(reader_t *reader, request_t *req);

/* number of requests in the trace */
int64_t get_num_of_req(reader_t *reader);

static inline trace_type_e get_trace_type(const reader_t *reader);
static inline bool obj_id_is_num(const reader_t *reader);

/* rewind so the trace can be read again */
void reset_reader(reader_t *reader);

/* clone a reader; the usual way to feed one trace to several threads */
reader_t *clone_reader(const reader_t *reader);

int close_reader(reader_t *reader);
static inline int close_trace(reader_t *reader);
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

A `request_t` is the container `read_one_req()` fills in. Allocate one up front and reuse it for the whole trace.

```c
static inline request_t *new_request(void);
static inline void copy_request(request_t *req_dest, const request_t *req_src);
static inline request_t *clone_request(const request_t *req);
static inline void free_request(request_t *req);
static inline void print_request(const request_t *req);
```

The fields you normally read are `obj_id`, `obj_size`, `clock_time`, `next_access_vtime` (oracle traces only), and `obj_cost`.

---

## Caches

Every eviction algorithm exposes an `_init` function taking the common parameters plus an optional algorithm-specific parameter string — the same string `cachesim` takes with `-e`.

```c
typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t hashpower;
  bool consider_obj_metadata;
} common_cache_params_t;

common_cache_params_t default_common_cache_params(void);

cache_t *LRU_init(common_cache_params_t ccache_params,
                  const char *cache_specific_params);
/* ... and FIFO_init, ARC_init, S3FIFO_init, Sieve_init, and the rest;
 * see libCacheSim/include/libCacheSim/evictionAlgo.h */
```

A `cache_t` is used through its function pointers:

```c
/* the whole interface: lookup plus on-demand insert and evict.
 * returns true on a cache hit */
bool (*get)(cache_t *, const request_t *);

/* look up without the insert/evict; update_cache controls whether the
 * lookup also updates state such as recency */
cache_obj_t *(*find)(cache_t *, const request_t *, bool update_cache);

bool (*can_insert)(cache_t *, const request_t *);
cache_obj_t *(*insert)(cache_t *, const request_t *);

/* which object would be evicted, without evicting it */
cache_obj_t *(*to_evict)(cache_t *, const request_t *);
void (*evict)(cache_t *, const request_t *);

/* user-triggered removal; eviction should go through evict instead */
bool (*remove)(cache_t *, obj_id_t);

void (*cache_free)(cache_t *);
```

Most programs only need `get()`. See [advanced_lib_extend.md](advanced_lib_extend.md) to implement a new algorithm.

---

## Simulator

Rather than driving the loop yourself, you can hand a trace and a cache to the simulator, which parallelizes across cache sizes or across caches.

```c
/* one cache, many sizes */
cache_stat_t *simulate_at_multi_sizes(reader_t *reader, const cache_t *cache,
                                      int num_of_sizes,
                                      const uint64_t *cache_sizes,
                                      reader_t *warmup_reader,
                                      double warmup_frac, int warmup_sec,
                                      int num_of_threads, bool use_random_seed);

/* one cache, sizes at a fixed step up to the working set size */
cache_stat_t *simulate_at_multi_sizes_with_step_size(
    reader_t *reader_in, const cache_t *cache_in, uint64_t step_size,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool use_random_seed);

/* many caches, each at its own configured size */
cache_stat_t *simulate_with_multi_caches(
    reader_t *reader, cache_t *caches[], int num_of_caches,
    reader_t *warmup_reader, double warmup_frac, int warmup_sec,
    int num_of_threads, bool free_cache_when_finish, bool use_random_seed);
```

Each returns an array with one `cache_stat_t` per simulation, which the caller frees:

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
  /* ... */
} cache_stat_t;
```

Object miss ratio is `n_miss / n_req`, and byte miss ratio is `n_miss_byte / n_req_byte`.

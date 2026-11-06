# Using libCacheSim as a library

`cachesim` covers most experiments. You need the library when you want something it can't do: a cache hierarchy, a cluster behind consistent hashing, a custom replay loop, or extra statistics collected request by request.

This page walks you from a ten-line simulator up to the parallel simulation API. For the full list of declarations, see the [C API reference](API.md). To write a new eviction algorithm, see [Adding algorithms and trace readers](advanced_lib_extend.md).

---

## Before you start

Build libCacheSim and install it, so the headers and the library are on your system:

```bash
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release
ninja -C _build
sudo ninja -C _build install
```

See the [README](/README.md#build-and-install-libcachesim) if the dependencies are not installed yet.

---

## Your first simulator

This program replays the sample trace through a 1 MiB LRU cache and prints the miss ratio:

```c
#include <libCacheSim.h>
#include <stdio.h>

int main(void) {
  /* 1. open the trace */
  reader_t *reader = open_trace("../data/cloudPhysicsIO.vscsi", VSCSI_TRACE, NULL);

  /* 2. allocate one request and reuse it for every read */
  request_t *req = new_request();

  /* 3. create a cache */
  common_cache_params_t cc_params = default_common_cache_params();
  cc_params.cache_size = 1 * MiB;
  cache_t *cache = LRU_init(cc_params, NULL);

  /* 4. feed every request to the cache */
  uint64_t n_req = 0, n_miss = 0;
  while (read_one_req(reader, req) == 0) {
    if (!cache->get(cache, req)) {
      n_miss++;
    }
    n_req++;
  }

  printf("miss ratio: %.4lf\n", (double)n_miss / n_req);

  /* 5. clean up */
  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);
  return 0;
}
```

Save it as `test.c`, then compile it:

```bash
gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o test.out -lm -lzstd
```

The trace path is relative, so run the program from a directory where `../data/` exists, such as `_build/`:

```bash
cd _build
/path/to/test.out
```

You should see `miss ratio: 0.8646`.

Every simulator follows these same steps: a **reader** produces **requests**, and you hand each request to a **cache**. The rest of this page covers each piece in more detail.

---

## Open a trace

`open_trace(path, type, params)` returns a `reader_t`. The third argument says how to parse the file. You can pass `NULL` for formats that describe themselves.

### Built-in formats

```c
reader_t *r1 = open_trace("../data/cloudPhysicsIO.vscsi", VSCSI_TRACE, NULL);
reader_t *r2 = open_trace("../data/cloudPhysicsIO.oracleGeneral.bin",
                          ORACLE_GENERAL_TRACE, NULL);
```

zstd-compressed files (`.zst`) work too. The reader decompresses them as it goes.

### Plain text

A text trace has one object id per line:

```c
reader_t *reader = open_trace("../data/cloudPhysicsIO.txt", PLAIN_TXT_TRACE, NULL);
```

### CSV

For a csv file, you tell the reader which column holds what. Columns are numbered from 1. The sample `data/cloudPhysicsIO.csv` has the header `version,time,op,size,lbn`, so time is column 2, size is column 4, and the object id is column 5:

```c
reader_init_param_t params = default_reader_init_params();
params.delimiter = ',';
params.time_field = 2;
params.obj_size_field = 4;
params.obj_id_field = 5;
params.obj_id_is_num = true;
params.obj_id_is_num_set = true;
params.has_header = true;
params.has_header_set = true;

reader_t *reader = open_trace("../data/cloudPhysicsIO.csv", CSV_TRACE, &params);
```

Three things trip people up here:

1. **Always start from `default_reader_init_params()`.** Several defaults are not zero: `cap_at_n_req` and `block_size` are `-1`, and `ignore_size_zero_req` is `true`. A struct literal like `{.time_field = 2}` silently sets them all to zero.
2. **`obj_id_is_num` defaults to `true` in the library.** That is the opposite of `cachesim`. If your id column holds strings, set it to `false` so the ids get hashed. Otherwise every id is parsed as a number, becomes `0`, and the whole trace collapses into a single object.
3. **Set the `_set` flag along with the value.** `has_header` and `obj_id_is_num` each have a matching `*_set` flag. Without it, the reader ignores your value and auto-detects.

### Binary with a format string

Any fixed-size binary record format can be described with a [Python struct](https://docs.python.org/3/library/struct.html) format string. This reads the vscsi sample as a generic binary trace:

```c
reader_init_param_t params = default_reader_init_params();
params.binary_fmt_str = "<IIIHHQQ";
params.obj_size_field = 2;
params.obj_id_field = 6;

reader_t *reader = open_trace("../data/cloudPhysicsIO.vscsi", BIN_TRACE, &params);
```

### Synthetic workloads

You can also generate requests instead of reading a file. See [synthetic traces](synthetic_traces.md):

```c
reader_t *reader = open_trace("Zipf_1000000_10000_0.9_42", SYNTHETIC_TRACE, NULL);
```

### Belady and other offline algorithms

Belady and BeladySize look into the future, so they need a trace that records when each object is requested next, such as `oracleGeneral`. If you write your own replay loop for them, set `params.require_valid_next_access = true` before opening the reader. This makes the reader reject traces where that future information has gone stale. See [oracle traces](oracle_traces.md) for the details.

### Other reader helpers

```c
int64_t n = get_num_of_req(reader);    /* number of requests in the trace */
reset_reader(reader);                  /* rewind to the start */
reader_t *copy = clone_reader(reader); /* independent copy, e.g. for another thread */
close_trace(reader);                   /* always close when done */
```

---

## Create a cache

Every eviction algorithm has an `_init` function with the same two arguments:

```c
common_cache_params_t cc_params = default_common_cache_params();
cc_params.cache_size = 64 * MiB;
cache_t *cache = S3FIFO_init(cc_params, NULL);
```

The common parameters are:

```c
typedef struct {
  uint64_t cache_size;         /* bytes, or number of objects if sizes are ignored */
  uint64_t default_ttl;
  int32_t hashpower;           /* log2 of the initial hash table size */
  bool consider_obj_metadata;  /* count per-object metadata against cache_size */
  int64_t n_total_req;         /* trace length, if known; 0 means unknown */
} common_cache_params_t;
```

`default_common_cache_params()` gives you a 1 GiB cache, `hashpower` 20, and metadata not counted. Note that `cachesim` uses `hashpower` 24 instead. The hash table grows on its own, so only change `hashpower` if you know your object count and want to avoid resizing or save memory.

The second argument of `_init` is an algorithm-specific parameter string. It is the same string you pass to `cachesim -e`, so `"n-seg=4"` for SLRU. Pass `NULL` for the defaults.

### Pick the algorithm by name

If the algorithm comes from a config file or the command line, use `create_cache_by_name()`. It accepts the same names as `cachesim` and returns `NULL` for an unknown name:

```c
cache_t *cache = create_cache_by_name("s3fifo", cc_params, NULL);
```

This function can construct any built-in algorithm, including the C++ ones, so link with `-lstdc++` as well:

```bash
gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o test.out -lm -lzstd -lstdc++
```

### Add an admission or prefetching policy

Attach these to an existing cache. They accept the same names as `cachesim -a` and `cachesim -p`:

```c
#include <libCacheSim/prefetchAlgo.h>  /* not included by libCacheSim.h */

cache->admissioner = create_admissioner("bloomfilter", NULL);
cache->prefetcher = create_prefetcher("OBL", NULL, cc_params.cache_size);
```

Admission names: `bloomfilter` (or `bloom-filter`), `prob` (or `probabilistic`), `size`, `sizeProbabilistic` (or `sizeProb`), and `adaptsize`. Prefetching names: `Mithril`, `OBL`, and `PG`.

### Free the cache

```c
cache->cache_free(cache);
```

---

## Send requests to the cache

### `get`: the one call most programs need

```c
bool hit = cache->get(cache, req);
```

`get` looks up the object. On a hit, it updates the cache state, for example by moving the object to the head of the LRU queue. On a miss, it inserts the object and evicts whatever it has to. It returns `true` for a hit.

### The lower-level calls

Sometimes you need to control each step yourself, for example to build an exclusive cache hierarchy. These calls split `get` into its parts:

| Call | What it does |
| --- | --- |
| `cache->find(cache, req, update_cache)` | Look up the object. Returns it, or `NULL` on a miss. Updates recency and similar state only if `update_cache` is `true`. |
| `cache->insert(cache, req)` | Insert the object. It does **not** evict, so make room first. Returns `NULL` if the object was not inserted, for example because it is larger than the cache. |
| `cache->evict(cache, req)` | Evict one object, chosen by the algorithm. |
| `cache->to_evict(cache, req)` | Report which object would be evicted next, without evicting it. |
| `cache->remove(cache, obj_id)` | Remove a specific object, as a user-triggered delete would. Returns `true` if it was there. Don't use this for eviction. |

> [!IMPORTANT]
> `get` increments `cache->n_req` for you. When you call the lower-level functions yourself, increment `cache->n_req` once per user request. Algorithms that measure time in requests ("virtual time") rely on it and misbehave without it.

---

## Run many simulations in parallel

Writing the loop yourself is fine for one cache. To sweep many cache sizes or algorithms, let the simulator run them across threads:

```c
/* one algorithm at several sizes */
uint64_t sizes[] = {1 * MiB, 64 * MiB, 1 * GiB};
cache_stat_t *res = simulate_at_multi_sizes(reader, cache, 3, sizes,
                                            NULL, 0, 0,  /* no warmup */
                                            4,           /* threads */
                                            false);      /* fixed random seed */
for (int i = 0; i < 3; i++) {
  printf("%ld: %.4f\n", (long)res[i].cache_size,
         (double)res[i].n_miss / res[i].n_req);
}
free(res);
```

There are three entry points:

* `simulate_at_multi_sizes()`: one cache, cloned at each size you list. The `cache` you pass is only a template, so you still free it yourself.
* `simulate_at_multi_sizes_with_step_size()`: one cache at sizes `step_size`, `2*step_size`, and so on, up to the template cache's size.
* `simulate_with_multi_caches()`: an array of caches you built, each with its own algorithm and size. Pass `free_cache_when_finish = true` to have the simulator free them.

Each call returns an array with one `cache_stat_t` per simulation, and you free the array. The object miss ratio is `n_miss / n_req`, and the byte miss ratio is `n_miss_byte / n_req_byte`.

To warm the caches up first, pass a `warmup_reader`, or a fraction (`warmup_frac`) or number of seconds (`warmup_sec`) of the main trace. Pass `NULL`, `0` and `0` to skip warmup. The exact signatures are in the [API reference](API.md#simulator).

---

## Trace statistics

`dist.h` computes per-request distances. It is not included by `libCacheSim.h`, so include it yourself:

```c
#include <libCacheSim/dist.h>

int64_t n = 0;

/* number of unique objects since the last access (STACK_DIST)
 * or until the next one (FUTURE_STACK_DIST); -1 if there is none */
int32_t *stack = get_stack_dist(reader, STACK_DIST, &n);

/* number of requests since the last (DIST_SINCE_LAST_ACCESS)
 * or first (DIST_SINCE_FIRST_ACCESS) access */
int32_t *dist = get_access_dist(reader, DIST_SINCE_LAST_ACCESS, &n);

free(stack);
free(dist);
```

Each array has one entry per request, and `n` receives the array length.

---

## Examples to copy from

The [example folder](/example) has complete, buildable programs. Each one has its own README with build steps:

| Example | Shows you how to |
| --- | --- |
| [cacheSimulator](/example/cacheSimulator) | write the basic replay loop above |
| [cacheSimulatorConcurrent](/example/cacheSimulatorConcurrent) | use `simulate_with_multi_caches()` and `simulate_at_multi_sizes()`, and open a csv trace |
| [cacheHierarchy](/example/cacheHierarchy) | feed the misses of several L1 caches into a shared L2 cache |
| [cacheCluster](/example/cacheCluster) | spread objects across a cluster of caches with consistent hashing |
| [plugin_v2](/example/plugin_v2) | load an eviction algorithm from a shared library (see the [plugin guide](quickstart_plugin.md)) |

---

## Troubleshooting

**`error while loading shared libraries: liblibCacheSim...`**: the dynamic linker hasn't picked up the newly installed library yet. Run `sudo ldconfig`, and check that the library is under `/usr/local/lib`.

**`pkg-config` can't find libCacheSim**: if you haven't run `ninja install`, point `pkg-config` at the build directory, which contains `libCacheSim.pc`:

```bash
export PKG_CONFIG_PATH=$PWD/_build
```

**Undefined references to `operator new` or `operator delete`**: you used something that pulls in a C++ algorithm, such as `create_cache_by_name()`. Add `-lstdc++`, or link with `g++`.

**Every object id is 0, or the miss ratio looks absurd on a csv trace**: your id column holds strings, but `obj_id_is_num` is still `true`. See [CSV](#csv) above.

---

## Advanced: optional hooks for request-aware algorithms

You can skip this section unless you are driving an algorithm that needs to know about *whole requests*, as LLM prefix caches do. Most algorithms see one object at a time and never use these hooks.

A `cache_t` has three optional function pointers. Each is `NULL` unless the algorithm installs it, so **always check before you call**:

```c
if (cache->set_request_ctx != NULL) { ... }
if (cache->record_request != NULL) { ... }
if (cache->evict_n != NULL) { ... }
```

`cachesim` and the test suite never call `set_request_ctx` or `record_request`, because a flat trace has no request boundaries. A driver that knows the boundaries does, for example [PrefixSim](https://github.com/cacheMon/PrefixSim), which lives in its own repository. If you write such a driver, follow the order below.

### 1. `set_request_ctx`: before making room

Call it at the start of each request, before you evict anything for it. It passes the properties of the request the algorithm is about to serve:

```c
cache_request_ctx_t ctx = {
    .timestamp = ts,             /* arrival time, seconds, as a double */
    .category = cat,             /* opaque workload class id; 0 = unknown */
    .n_blocks = n,               /* how many objects the request touches */
    .session = sid,              /* opaque conversation id; 0 = unknown */
    .session_next_access = nxt,  /* index of the session's next request, INT64_MAX if none */
};
cache->set_request_ctx(cache, &ctx);
```

Why each field exists:

* `timestamp` lets a policy whose scores decay over real time, such as `AsymCache`, know "now" while it chooses victims. It is a `double` because `request_t::clock_time` holds whole seconds. KV blocks live for around 100 s, so whole seconds would make every block arriving in the same second tie.
* `category` lets a policy keep separate statistics per workload class, as `WorkloadAware` does. What counts as a class is up to the trace. The driver folds whatever fields the trace has into one id, and the algorithm only compares ids for equality.
* `session` and `session_next_access` let a session-level policy group objects by conversation. `session_next_access` is the session-level counterpart of `next_access_vtime`, for oracle policies.

### 2. `record_request`: after making room, before the accesses

```c
cache->record_request(cache, obj_ids, n_obj_ids);
```

This passes the request's full object sequence, in order. For a prefix cache, that sequence is a path in the prefix tree, which the algorithm cannot rebuild from individual accesses. Call it once per request, after you have made room and before you replay the accesses. At that point the new objects are not in the cache yet, and that is on purpose. The algorithm learns the structure first, and the inserts that follow mark the objects as resident.

The two hooks exist separately because of *when* they run. `record_request` runs after eviction, so an algorithm that read it during eviction would see the *previous* request. `set_request_ctx` runs before eviction, so it can deliver what eviction needs.

### 3. `evict_n`: evict several objects at once

```c
if (cache->evict_n != NULL) {
  cache->evict_n(cache, req, n);    /* returns how many were evicted */
} else {
  while (still_need_room()) cache->evict(cache, req);
}
```

`n` is a **hard cap**, not a hint. You asked for room for `n` objects, and evicting more would silently throw away objects nothing asked to free.

Algorithms implement `evict_n` for two reasons. Sampling-based algorithms pay their sampling cost once per call, so one call for `n` objects is much cheaper than `n` calls. On `qwen_coder` with 8k blocks and 128 samples, `PartialNodeRandomFreq` runs 6x faster through `evict_n` than through the loop, with the same hit ratio to four decimals. The second reason is that some algorithms want to evict related objects together, such as part of one prefix-tree node.

The algorithms that use these hooks are in [cache/eviction/cpp/](/libCacheSim/cache/eviction/cpp/). To write one, see [Adding algorithms and trace readers](advanced_lib_extend.md#advanced-algorithms-that-need-whole-request-information).

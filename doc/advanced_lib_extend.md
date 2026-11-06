# Adding algorithms and trace readers

This page is for you if you want libCacheSim to run your own eviction algorithm or read a new trace format. It covers which approach to pick, the steps for each, and the rules that are easy to get wrong.

---

## Pick an approach

| You want to | Use | Rebuild libCacheSim? |
| --- | --- | --- |
| Try an idea quickly, or keep your algorithm out of the source tree | a [plugin](quickstart_plugin.md), in Python, C, or C++ | No |
| Add an algorithm that ships with libCacheSim, runs at full speed, and works everywhere `cachesim` does | an in-tree algorithm in [C](#add-an-eviction-algorithm-in-c) or [C++](#write-it-in-c) | Yes |
| Read a trace format libCacheSim doesn't know | a [new trace reader](#add-a-trace-reader) | Yes |

Plugins are the fastest way to start. Move an algorithm in-tree once you want to contribute it, or once you need hooks that plugins don't expose.

---

## Add an eviction algorithm in C

The easiest start is to copy an existing algorithm. [LRU](/libCacheSim/cache/eviction/LRU.c) is short and makes a good template. Every algorithm implements this interface (shown with LRU's names):

```c
/* set up the cache and its state; cache_specific_params is the -e string */
cache_t *LRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

/* release everything the cache allocated */
void LRU_free(cache_t *cache);

/* find, plus insert and evict on a miss; return true on a hit */
bool LRU_get(cache_t *cache, const request_t *req);

/* look up an object; update state such as recency only if update_cache is true */
cache_obj_t *LRU_find(cache_t *cache, const request_t *req, bool update_cache);

/* insert an object that is not in the cache yet; does not evict */
cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);

/* report which object would be evicted next, without evicting it */
cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req);

/* evict one object; don't rely on req here */
void LRU_evict(cache_t *cache, const request_t *req);

/* remove a specific object, as a user delete would; not for eviction */
bool LRU_remove(cache_t *cache, obj_id_t obj_id);
```

Then wire it in. Using `myCache` as the example name:

1. **Write the implementation.** Add `myCache.c` to [cache/eviction/](/libCacheSim/cache/eviction/).
2. **Add per-object metadata, if you need it.** Add a struct for it to the union in [include/libCacheSim/cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h).
3. **Declare the constructor.** Add `myCache_init()` to [include/libCacheSim/evictionAlgo.h](/libCacheSim/include/libCacheSim/evictionAlgo.h).
4. **Compile it.** Add `eviction/myCache.c` to [cache/CMakeLists.txt](/libCacheSim/cache/CMakeLists.txt).
5. **Give it a name.** Add `{"mycache", myCache_init}` to the table in [cache/cacheAlgoRegistry.c](/libCacheSim/cache/cacheAlgoRegistry.c). The table is alphabetical. This one entry makes the algorithm available to `cachesim`, to the MINISIM mode of `mrcProfiler`, and to `create_cache_by_name()`.
6. **Test and document it.** Add a test to [test/test_evictionAlgo.c](/test/test_evictionAlgo.c), and list the algorithm in the [README](/README.md#supported-algorithms) with the name users type. If it takes `-e` parameters, add a case to the `testCLI` target too, as described in [CONTRIBUTING.md](/CONTRIBUTING.md#testing).

Then rebuild and try it:

```bash
ninja -C _build
cd _build && ./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi mycache 1gb
```

> [!TIP]
> Many algorithms keep their state in a doubly linked list. [cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h) has helpers for manipulating one, so you don't have to write your own.

> [!TIP]
> Algorithms can be built from other algorithms. LeCaR, for example, runs one LRU and one LFU internally. See [LeCaRv0.c](/libCacheSim/cache/eviction/LeCaRv0.c) for how to compose caches this way.

---

## Write it in C++

Most algorithms are in C, but C++ is fine too. Several were ported from their authors' C++ code, such as [LHD](/libCacheSim/cache/eviction/LHD/) and [LRB](/libCacheSim/cache/eviction/LRB/). For small, self-contained examples, look at [LFU.cpp](/libCacheSim/cache/eviction/cpp/LFU.cpp) and [LRU_K.cpp](/libCacheSim/cache/eviction/cpp/LRU_K.cpp) in [cache/eviction/cpp/](/libCacheSim/cache/eviction/cpp/).

The pattern is:

1. Write the algorithm as an ordinary C++ class.
2. Expose the C interface above (`myCache_init()`, `myCache_get()`, and the rest) inside an `extern "C"` block. These functions forward to your class.

Then follow the same wiring steps as for a C algorithm.

---

## Add a trace reader

libCacheSim already reads [txt](/libCacheSim/traceReader/generalReader/txt.c), [csv](/libCacheSim/traceReader/generalReader/csv.c), and generic binary traces. Before you write a reader, check whether a binary format string already describes your format (see [Binary with a format string](advanced_lib.md#binary-with-a-format-string)). If you can, convert the trace once with `traceConv` and use `oracleGeneral` from then on. Binary traces are faster to replay and more compact, and zstd-compressed binary traces can be read without decompressing them first.

If you do need a new reader, you implement two functions:

```c
/* set up the reader; return 0 on success, 1 on failure */
int myReader_setup(reader_t *reader);

/* read the next request into req; return 0 on success, 1 at end of trace */
int myReader_read_one_req(reader_t *reader, request_t *req);
```

For a compact example, see [twrBin.h](/libCacheSim/traceReader/customizedReader/twrBin.h). For the formats used by the sample traces in [data/](/data/), see [vscsi.h](/libCacheSim/traceReader/customizedReader/vscsi.h) and [oracleGeneralBin.h](/libCacheSim/traceReader/customizedReader/oracle/oracleGeneralBin.h).

To wire it in:

1. **Add a trace type.** Add `MYREADER_TRACE` to `trace_type_e` in [include/libCacheSim/enum.h](/libCacheSim/include/libCacheSim/enum.h). Add its name to `g_trace_type_name` in the same file, at the same position, because the array is indexed by the enum.
2. **Write the reader.** Add `myReader.h` to [traceReader/customizedReader/](/libCacheSim/traceReader/customizedReader/) with the two functions.
3. **Call it.** Add a `case MYREADER_TRACE` that calls `myReader_setup()` in `setup_reader()` in [traceReader/reader.c](/libCacheSim/traceReader/reader.c), and one that calls `myReader_read_one_req()` in `read_one_req()` in [traceReader/readerRequest.c](/libCacheSim/traceReader/readerRequest.c).
4. **Make it reachable from the command line.** Map a name to `MYREADER_TRACE` in `trace_type_str_to_enum()` in [bin/cli_reader_utils.c](/libCacheSim/bin/cli_reader_utils.c).

---

## Advanced: algorithms that need whole-request information

You can skip this section unless you are building an LLM prefix-cache policy, or another algorithm that needs to know which objects belong to the same request. These algorithms rely on the optional `record_request`, `set_request_ctx`, and `evict_n` hooks. [The library guide](advanced_lib.md#advanced-optional-hooks-for-request-aware-algorithms) explains them from the caller's side.

Keep in mind that only a driver that knows request boundaries calls these hooks. [PrefixSim](https://github.com/cacheMon/PrefixSim) does. `cachesim` and the test suite do not, because a flat trace has no request boundaries to report.

### Partial-node (prefix-tree) algorithms

Prefix-cache algorithms evict a *prefix-tree node* at a time, not one independent object. A node is a run of blocks with no branch in the middle. [`eviction::PartialNodeCache`](/libCacheSim/cache/eviction/cpp/partialNodeCache.hpp) already does everything this family has in common: it keeps the prefix tree, samples nodes uniformly, selects candidates, and installs the whole `cache_t` interface, including `record_request` and a batched `evict_n`.

A new sample-based variant only has to supply a score:

```cpp
class MyPolicy : public eviction::PartialNodeCache {
 public:
  double score(const cache_t *cache, const cache_obj_t *obj) const override {
    return ...;  // lower is evicted first
  }
};

cache_t *MyPolicy_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params) {
  return eviction::partial_node_cache_init("MyPolicy", ccache_params,
                                           cache_specific_params,
                                           new eviction::MyPolicy());
}
```

If your policy needs per-request state beyond the tree, override `on_record_request()` as well. [PartialNodeRandomCompute.cpp](/libCacheSim/cache/eviction/cpp/PartialNodeRandomCompute.cpp) is a complete example. Register the new algorithm with the same steps as any [C algorithm](#add-an-eviction-algorithm-in-c).

These algorithms also need block ids to be unique per prefix, so that each block sits in exactly one node. The tree counts any violations and warns about them.

### Policies that need per-request metadata

Some policies score a block by a property of the request it arrived with, such as its arrival time or its traffic class, rather than by anything the block itself carries. Two hooks deliver that information, at different times:

| Hook | Delivers | Called |
| --- | --- | --- |
| `set_request_ctx` | a `cache_request_ctx_t`: `timestamp`, `category`, `n_blocks`, `session`, `session_next_access` | **before** the caller makes room for the request |
| `record_request` | the request's object ids, in order | **after** the caller makes room |

Use `set_request_ctx` when eviction itself needs the value. `record_request` arrives too late for that: during the eviction that makes room for request *N*, the most recent `record_request` was for request *N−1*. Use `record_request` when you need an object's position within the request, or the request's path through the tree. Many policies need both. [WorkloadAware.cpp](/libCacheSim/cache/eviction/cpp/WorkloadAware.cpp), for example, takes the workload class from `set_request_ctx` and the prefix offset from `record_request`.

Two rules are easy to get wrong:

1. **Keep working when neither hook is called.** Under `cachesim` and the tests, they never are. If your only clock advances inside `record_request`, it never advances there, every score ties, and the policy quietly turns into insertion order. Give the policy a per-access fallback, and switch it off with a flag the first time a hook fires. `WorkloadAware` and `AsymCache` both do this. Without the hooks they fall back to a single class and their own internal clock, which is weaker but still correct.
2. **`record_request` names objects that are not in the cache yet.** It runs before the inserts, so some of the ids it reports have no cache object behind them. Keep the set of *known* objects separate from the set of *resident* ones, and pick victims only from the resident set. Otherwise eviction will eventually return an id the hash table has never seen.

One more, if your scores can tie: break ties in a fixed order that doesn't depend on the hash table's layout. Exponential scores reach exactly zero in double precision more often than you'd expect (`exp(-x)` underflows once x is above about 745), so ties are not rare. `WorkloadAware` keeps an explicit first-seen list of workload classes for this reason.

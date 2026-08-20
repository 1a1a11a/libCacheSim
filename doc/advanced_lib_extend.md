# Extend libCacheSim 


## Add new eviction algorithms in C
Implementing a new eviction algorithm is easy in libCacheSim. You can start from [LRU](/libCacheSim/cache/eviction/LRU.c) as the template, copy to `myCache.c` and change the functions. The cache interface requires the developer to implement the following functions: 

```c
/* initialize all the variables */
cache_t *LRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);

/* free the resources used by the cache */
void LRU_free(cache_t *cache);

/* get the object from the cache, it is find + on-demand insert/evict, return true if cache hit */
bool LRU_get(cache_t *cache, const request_t *req);

/* find an object in the cache, return the cache object if found, NULL otherwise, update_cache means whether update the cache state, e.g., moving object to the head of the queue */
cache_obj_t *LRU_find(cache_t *cache, const request_t *req, bool update_cache);

/* insert an object to the cache, return the cache object, this assumes the object is not in the cache */
cache_obj_t *LRU_insert(cache_t *cache, const request_t *req);

/* find the object to be evicted, return the cache object, not used very often */
cache_obj_t *LRU_to_evict(cache_t *cache, const request_t *req);

/* evict an object from the cache, req should not be used */
void LRU_evict(cache_t *cache, const request_t *req);

/* remove an object from the cache, return true if the object is found and removed, note that this is used for user-triggered remove, eviction should use evict */
bool LRU_remove(cache_t *cache, obj_id_t obj_id);
```

Specifically, you can following the steps:
1. Add a new file e.g., `mycache.c` to [cache/eviction/](/libCacheSim/cache/eviction/) for your cache eviction algorithm implementation. 
2. If your cache eviction algorithm needs extra metadata, add a new object metadata struct in 
   [include/libCacheSim/cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h).
3. Add `myCache_init()` function to [include/libCacheSim/evictionAlgo.h](/libCacheSim/include/libCacheSim/evictionAlgo.h).
4. Add mycache.c to [CMakeLists.txt](/libCacheSim/cache/CMakeLists.txt) so that it can be compiled.
5. Add command line option in [bin/cachesim/cache_init.h](/libCacheSim/bin/cachesim/cache_init.h) so that you can use `cachesim` binary. You may also want to take a look at [bin/cachesim/cli_parser.c](/libCacheSim/bin/cachesim/cli_parser.c). 
6. Remember to add a test in [test/test_evictionAlgo.c](/test/test_evictionAlgo.c) and add the algorithm to the [README](/README.md#supported-algorithms). 

> [!TIP]
> Many eviction algorithms use a doubly linked list to maintain state, libCacheSim provides several functions in [cacheObj.h](/libCacheSim/include/libCacheSim/cacheObj.h) to manipulate list. 


> [!TIP]
> Many eviction algorithms are composable, e.g., LeCaR uses one LRU and one LFU, it is easy to support these algorithms in libCacheSim, please take a look at [LeCaR](/libCacheSim/cache/eviction/LeCaRv0.c). 


---

## Add new eviction algorithms in C++
While most of the eviction algorithms in libCacheSim is written in C, there are a few written in C++, especially the ones that are ported from the original user, e.g., [LHD](/libCacheSim/cache/eviction/LHD/), [LRB](/libCacheSim/cache/eviction/LRB/).

You can also write your eviction algorithm in C++ and use it in libCacheSim. We have provided a few C++ eviction algorithms as examples in [cache/eviction/cpp/](/libCacheSim/cache/eviction/cpp/), e.g., [LFU](/libCacheSim/cache/eviction/cpp/LFU.cpp), [LRU-K](/libCacheSim/cache/eviction/cpp/LRU_K.cpp).

There are two steps you can follow, 
1. implement most functions in C++ 
2. implement the libCacheSim cache interface in C, e.g., `mycache_init()`, `mycache_get()`. 



---

## Add a partial-node (prefix-tree) eviction algorithm

Prefix-cache algorithms evict at the granularity of a *prefix-tree node* -- a
maximal non-branching run of blocks -- rather than a single independent object.
`eviction::PartialNodeCache` in
[cache/eviction/cpp/partialNodeCache.hpp](/libCacheSim/cache/eviction/cpp/partialNodeCache.hpp)
already owns everything that is common to that family: the prefix tree, uniform
node sampling, candidate selection, and the whole `cache_t` vtable including
`record_request` and the batched `evict_n`.

A new sample-based variant only supplies a score:

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

Override `on_record_request()` as well if the policy needs per-request state
beyond the tree. See
[PartialNodeRandomCompute.cpp](/libCacheSim/cache/eviction/cpp/PartialNodeRandomCompute.cpp)
for a complete example; then register it exactly as in the section above.

These algorithms only work under a driver that calls `cache->record_request()`
-- `prefixsim` does, `cachesim` does not, because a flat trace has no request
boundaries to report. They also require block ids to be prefix-unique, so that a
block occupies exactly one node; the tree counts violations and warns.

---

## Add an algorithm that needs per-request metadata

Some policies score a block by a property of the *request* it arrived with rather
than by anything the block itself carries -- the wall-clock time of the arrival,
or which class of traffic it belongs to. Two optional `cache_t` slots deliver
that, and which one you want depends entirely on when you need the value:

| slot | delivers | called |
|---|---|---|
| `set_request_ctx` | `cache_request_ctx_t` -- `timestamp`, `category`, `n_blocks` | *before* the caller makes room |
| `record_request` | the object id sequence, in request order | *after* it makes room |

Install `set_request_ctx` when eviction itself must consult the value, because
`record_request` runs too late: during the eviction that makes room for request
*N*, the last `record_request` was request *N-1*. Install `record_request` when
you need position within the request, or the path structure. Many policies want
both — see
[WorkloadAware.cpp](/libCacheSim/cache/eviction/cpp/WorkloadAware.cpp), which
takes the workload class from one and the prefix offset from the other.

Two rules that are easy to get wrong:

1. **Stay correct when neither hook fires.** Only a driver that knows request
   boundaries calls them; `cachesim` cannot, and neither can the test suite. If
   your only clock advances inside `record_request`, then under those drivers it
   never advances, every score ties, and the policy silently decays to insertion
   order. Give it a per-access fallback and flip a flag the first time a hook
   fires, so the fallback stops as soon as the real thing arrives.
2. **`record_request` names objects that are not resident yet.** It runs before
   the inserts, so an id it reports may have no cache object behind it. Keep the
   "known" set and the "resident" set distinct and only ever choose victims from
   the latter; otherwise eviction eventually hands the caller an id the hash table
   has never seen.

One more, if your score can tie: resolve ties in a deterministic order that does
not depend on a hash table's layout. Exponential scores reach *exactly* zero in
double precision surprisingly often (`exp(-x)` underflows at x > ~745), so ties
are not the rare event they look like -- `WorkloadAware` keeps an explicit
first-seen list of workload classes for this reason.

---

## Add new trace readers 
libCacheSim supports [txt](/libCacheSim/traceReader/generalReader/txt.c), [csv](/libCacheSim/traceReader/generalReader/csv.c), and binary traces. We prefer binary traces because it allows libCacheSim to run faster, and the traces are more compact. 
For binary traces, libCacheSim also supports zstd compressed traces without decompression.

But if you ever need to implement a new trace type, see [twrBin.h](/libCacheSim/traceReader/customizedReader/twrBin.h) for a compact example reader, or [vscsi.h](/libCacheSim/traceReader/customizedReader/vscsi.h) and [oracleGeneralBin.h](/libCacheSim/traceReader/customizedReader/oracle/oracleGeneralBin.h) for the formats used by the sample traces in [data/](/data/). 

To implement a reader, you need to implement two functions:
```c
/* initialize the reader, return 0 if success, 1 otherwise */
int myReader_setup(reader_t *reader);

/* read one request from the trace, return 0 if success, 1 otherwise */
int myReader_read_one_req(reader_t *reader, request_t *req);

```

Here are the steps to add a new trace reader:
1. add a new new trace type, e.g., `MYREADER_TRACE` in `trace_type_e` in [include/libCacheSim/enum.h](/libCacheSim/include/libCacheSim/enum.h). 
2. add a new reader file, e.g., `myReader.h` in [traceReader/customizedReader/](/libCacheSim/traceReader/customizedReader/) and implement the two functions.
3. add `myReader_setup()` to `setup_reader()` in [traceReader/reader.c](/libCacheSim/traceReader/reader.c), and `myReader_read_one_req()` to `read_one_req()` in [traceReader/readerRequest.c](/libCacheSim/traceReader/readerRequest.c).
4. add `MYREADER_TRACE` to `trace_type_str_to_enum()` in [bin/cli_reader_utils.c](/libCacheSim/bin/cli_reader_utils.c)



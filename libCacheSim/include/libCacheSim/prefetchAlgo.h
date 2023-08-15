#ifndef PREFETCHINGALGO_H
#define PREFETCHINGALGO_H
#include "cache.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prefetcher;
typedef struct prefetcher *(*prefetcher_create_func_ptr)(const char *);
typedef void (*cache_insert_prefetch_func_ptr)(cache_t *, const request_t *,
                                               bool);
typedef void (*prefetcher_free_func_ptr)(struct prefetcher *);

typedef struct prefetcher {
  void *params;
  // for general purpose,
  // not only prefetch, but also insert req and evict when space is full
  cache_insert_prefetch_func_ptr insert_prefetch;
  prefetcher_free_func_ptr free;
} prefetcher_t;

prefetcher_t *create_Mithril_prefetcher(const char *init_paramsm,
                                        uint64_t cache_size);

static inline prefetcher_t *create_prefetcher(const char *prefetching_algo,
                                              const char *prefetching_params,
                                              uint64_t cache_size) {
  prefetcher_t *prefetcher = NULL;
  if (strcasecmp(prefetching_algo, "Mithril") == 0) {
    prefetcher = create_Mithril_prefetcher(prefetching_params, cache_size);
  } else {
    ERROR("prefetching algo %s not supported\n", prefetching_algo);
  }

  return prefetcher;
}

#ifdef __cplusplus
}
#endif
#endif
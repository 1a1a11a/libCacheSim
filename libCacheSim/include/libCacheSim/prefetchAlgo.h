#ifndef PREFETCHINGALGO_H
#define PREFETCHINGALGO_H

#include <strings.h>

#include "cache.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prefetcher;
typedef struct prefetcher *(*prefetcher_create_func_ptr)(const char *);
typedef void (*prefetcher_prefetch_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_handle_find_func_ptr)(cache_t *, const request_t *,
                                                bool);
typedef void (*prefetcher_handle_insert_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_handle_evict_func_ptr)(cache_t *, const request_t *);
typedef void (*prefetcher_free_func_ptr)(struct prefetcher *);
typedef struct prefetcher *(*prefetcher_clone_func_ptr)(struct prefetcher *,
                                                        uint64_t);

typedef struct prefetcher {
  void *params;
  void *init_params;
  prefetcher_prefetch_func_ptr prefetch;
  prefetcher_handle_find_func_ptr handle_find;
  prefetcher_handle_insert_func_ptr handle_insert;
  prefetcher_handle_evict_func_ptr handle_evict;
  prefetcher_free_func_ptr free;
  prefetcher_clone_func_ptr clone;
} prefetcher_t;

prefetcher_t *create_Mithril_prefetcher(const char *init_paramsm,
                                        uint64_t cache_size);
prefetcher_t *create_OBL_prefetcher(const char *init_paramsm, uint64_t cache_size);
prefetcher_t *create_PG_prefetcher(const char *init_paramsm, uint64_t cache_size);

static inline prefetcher_t *create_prefetcher(const char *prefetching_algo,
                                              const char *prefetching_params,
                                              uint64_t cache_size) {
  prefetcher_t *prefetcher = NULL;
  if (strcasecmp(prefetching_algo, "Mithril") == 0) {
    prefetcher = create_Mithril_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "OBL") == 0) {
    prefetcher = create_OBL_prefetcher(prefetching_params, cache_size);
  } else if (strcasecmp(prefetching_algo, "PG") == 0) {
    prefetcher = create_PG_prefetcher(prefetching_params, cache_size);
  } else {
    ERROR("prefetching algo %s not supported\n", prefetching_algo);
  }

  return prefetcher;
}

#ifdef __cplusplus
}
#endif
#endif
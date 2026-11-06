#include <math.h>

#include "cache/cacheUtils.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/plugin.h"
#include "libCacheSim/simulator.h"
#include "mrc_internal.h"
#include "profiler/profilerInternal.h"
#include "utils/include/myprint.h"
#include "utils/include/mystr.h"

cache_stat_t *generate_mini_mrc(struct MINI_arguments *args) {
  int n_caches = args->n_cache_size * args->n_eviction_algo;
  reader_t **readers = my_malloc_n(reader_t *, n_caches);
  for (int i = 0; i < n_caches; i++) {
    if (args->cache_size_ratio[i] == 0) {
      args->cache_size_ratio[i] = args->cache_size_ratio[0];
    }
    args->reader->init_params.sampler =
        create_SHARDS_sampler(args->cache_size_ratio[i]);
    readers[i] = clone_reader(args->reader);
  }
  /* readers[i] keeps a pointer to the sampler it was cloned with in its
   * init_params, and the simulation clones readers[i], and so that sampler,
   * again: the samplers are freed with the readers once it is done. The base
   * reader itself does not sample. */
  args->reader->init_params.sampler = NULL;

  cache_stat_t *result = simulate_with_multi_caches_scaling(
      readers, args->caches, n_caches, NULL, 0, args->warmup_sec,
      args->n_thread, true);

  for (int i = 0; i < n_caches; i++) {
    sampler_t *sampler = readers[i]->init_params.sampler;
    close_reader(readers[i]);
    sampler->free(sampler);
  }
  my_free(sizeof(reader_t *) * n_caches, readers);
  return result;
}

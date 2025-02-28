#include <math.h>

#include "../../cache/cacheUtils.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/plugin.h"
#include "../../include/libCacheSim/simulator.h"
#include "../../profiler/simulator.c"
#include "../../utils/include/myprint.h"
#include "../../utils/include/mystr.h"
#include "mrc_internal.h"

cache_stat_t *generate_mini_mrc(struct MINI_arguments *args) {
  reader_t **readers = my_malloc_n(reader_t *, args->n_cache_size * args->n_eviction_algo);
  for (int i = 0; i < args->n_cache_size * args->n_eviction_algo; i++) {
    if (args->cache_size_ratio[i] == 0) {
      args->cache_size_ratio[i] = args->cache_size_ratio[0];
    }
    args->reader->init_params.sampler = create_SHARDS_sampler(args->cache_size_ratio[i]);
    readers[i] = clone_reader(args->reader);
  }
  cache_stat_t *result = simulate_with_multi_caches_scaling(readers, args->caches, args->n_cache_size * args->n_eviction_algo,
                                                    NULL, 0, args->warmup_sec, args->n_thread, true);
  return result;
}

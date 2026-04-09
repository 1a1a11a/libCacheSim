#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "cachesim/internal.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/simulator.h"

static double ratio_u64(uint64_t numerator, uint64_t denominator) {
  return denominator > 0 ? (double)numerator / (double)denominator : 0.0;
}

static double extra_write_ratio(const cache_t *cache, uint64_t req_bytes,
                                const char **extra_label) {
  *extra_label = "none";
  if (req_bytes == 0) {
    return 0.0;
  }

  if (strcasecmp(cache->cache_name, "FIFO") == 0) {
    return 0.0;
  }

  if (strncasecmp(cache->cache_name, "Clock", 5) == 0) {
    const Clock_params_t *params =
        (const Clock_params_t *)cache->eviction_params;
    *extra_label = "byte_reinsertion_ratio";
    return ratio_u64(params->n_byte_rewritten, req_bytes);
  }

  if (strncasecmp(cache->cache_name, "LRU", 3) == 0) {
    const LRU_params_t *params = (const LRU_params_t *)cache->eviction_params;
    *extra_label = "byte_promotion_ratio";
    return ratio_u64(params->n_byte_promoted, req_bytes);
  }

  fprintf(stderr, "unsupported cache for flash metrics: %s\n",
          cache->cache_name);
  exit(1);
}

int main(int argc, char **argv) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  int num_caches = args.n_cache_size * args.n_eviction_algo;
  cache_stat_t *result = simulate_with_multi_caches(
      args.reader, args.caches, num_caches, NULL, 0, args.warmup_sec,
      args.n_thread, false, true);

  printf("RESULT,trace,cache_name,cache_size_bytes,req_miss_ratio,"
         "byte_miss_ratio,extra_ratio_label,extra_ratio,flash_write_ratio\n");
  for (int i = 0; i < num_caches; i++) {
    const char *extra_label = "none";
    double req_miss_ratio = ratio_u64(result[i].n_miss, result[i].n_req);
    double byte_miss_ratio =
        ratio_u64(result[i].n_miss_byte, result[i].n_req_byte);
    double extra_ratio =
        extra_write_ratio(args.caches[i], result[i].n_req_byte, &extra_label);
    double flash_write_ratio = byte_miss_ratio + extra_ratio;

    printf("RESULT,%s,%s,%llu,%.8f,%.8f,%s,%.8f,%.8f\n", args.trace_path,
           args.caches[i]->cache_name,
           (unsigned long long)result[i].cache_size, req_miss_ratio,
           byte_miss_ratio, extra_label, extra_ratio, flash_write_ratio);
  }

  for (int i = 0; i < num_caches; i++) {
    args.caches[i]->cache_free(args.caches[i]);
  }
  my_free(sizeof(cache_stat_t) * num_caches, result);
  free_arg(&args);

  return 0;
}

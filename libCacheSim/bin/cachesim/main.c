

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/simulator.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

int main(int argc, char **argv) {
  struct arguments args;
  parse_cmd(argc, argv, &args);
  if (args.n_cache_size == 0) {
    WARN("no cache size found\n");
    exit(0);
  }

  if (args.n_cache_size == 1) {
    simulate(args.reader, args.cache, args.warmup_sec, args.ofilepath);
    return 0;
  }

  // cache_stat_t *result = simulate_at_multi_sizes(
  //     args.reader, args.cache, args.n_cache_size, args.cache_sizes, NULL, 0,
  //     args.warmup_sec, args.n_thread);

  cache_t **caches = malloc(sizeof(cache_t *) * args.n_cache_size);
  for (int i = 0; i < args.n_cache_size; i++) {
    caches[i] = create_cache_with_new_size(args.cache, args.cache_sizes[i]);
  }
  cache_stat_t *result =
      simulate_with_multi_caches(args.reader, caches, args.n_cache_size, NULL,
                                 0, args.warmup_sec, args.n_thread);

  char output_str[1024];
  char output_filename[128];
  create_dir("result/");
  sprintf(output_filename, "result/%s", rindex(args.trace_path, '/') + 1);
  FILE *output_file = fopen(output_filename, "a");

  uint64_t size_unit = 1;
  char *size_unit_str = "";
  if (!args.ignore_obj_size) {
    if (args.cache_sizes[0] > GiB) {
      size_unit = GiB;
      size_unit_str = "GiB";
    } else if (args.cache_sizes[0] > MiB) {
      size_unit = MiB;
      size_unit_str = "MiB";
    } else if (args.cache_sizes[0] > KiB) {
      size_unit = KiB;
      size_unit_str = "KiB";
    }
  }

  printf("\n");
  for (int i = 0; i < args.n_cache_size; i++) {
    snprintf(output_str, 1024,
             "%s %s cache size %8ld%s, %lld req, miss ratio %.4lf, byte miss "
             "ratio %.4lf\n",
             output_filename, args.cache->cache_name,
             (long)result[i].cache_size / size_unit, size_unit_str,
             (long long)result[i].n_req,
             (double)result[i].n_miss / (double)result[i].n_req,
             (double)result[i].n_miss_byte / (double)result[i].n_req_byte);
    printf("%s", output_str);
    fprintf(output_file, "%s", output_str);
  }
  fclose(output_file);

  close_reader(args.reader);
  args.cache->cache_free(args.cache);
  for (int i = 0; i < args.n_cache_size; i++) {
    caches[i]->cache_free(caches[i]);
  }

  if (args.eviction_params != NULL) {
    free(args.eviction_params);
  }
  if (args.admission_params != NULL) {
    free(args.admission_params);
  }

  return 0;
}

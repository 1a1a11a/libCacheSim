

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>
#include <libgen.h>

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
    ERROR("no cache size found\n");
  }

  if (args.n_cache_size * args.n_eviction_algo == 1) {
    simulate(args.reader, args.caches[0], args.report_interval, args.warmup_sec,
             args.ofilepath);

    free_arg(&args);
    return 0;
  }

  // cache_stat_t *result = simulate_at_multi_sizes(
  //     args.reader, args.cache, args.n_cache_size, args.cache_sizes, NULL, 0,
  //     args.warmup_sec, args.n_thread);

  cache_stat_t *result = simulate_with_multi_caches(
      args.reader, args.caches, args.n_cache_size * args.n_eviction_algo, NULL,
      0, args.warmup_sec, args.n_thread, true);

  char output_str[1024];
  char output_filename[128];
  create_dir("result/");
  sprintf(output_filename, "result/%s", basename(args.trace_path));
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
  for (int i = 0; i < args.n_cache_size * args.n_eviction_algo; i++) {
    snprintf(output_str, 1024,
             "%s %32s cache size %8ld%s, %lld req, miss ratio %.4lf, byte miss "
             "ratio %.4lf\n",
             output_filename, result[i].cache_name,
             (long)(result[i].cache_size / size_unit), size_unit_str,
             (long long)result[i].n_req,
             (double)result[i].n_miss / (double)result[i].n_req,
             (double)result[i].n_miss_byte / (double)result[i].n_req_byte);
    printf("%s", output_str);
    fprintf(output_file, "%s", output_str);
  }
  fclose(output_file);

  free_arg(&args);

  return 0;
}

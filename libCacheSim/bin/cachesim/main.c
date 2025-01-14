

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
    simulate(args.reader, args.caches[0], args.report_interval, args.warmup_sec, args.ofilepath, args.ignore_obj_size,
             args.print_head_req);

    free_arg(&args);
    return 0;
  }

  cache_stat_t *result = simulate_with_multi_caches(
      args.reader, args.caches, args.n_cache_size * args.n_eviction_algo, NULL,
      0, args.warmup_sec, args.n_thread, true, true);

  // output to file
  char output_str[1024];
  // ensure file path exists
  char *output_dir = rindex(args.ofilepath, '/');
  if (output_dir != NULL) {
    size_t dir_length = output_dir - args.ofilepath;
    char dir_path[1024];
    snprintf(dir_path, dir_length + 1, "%s", args.ofilepath);
    create_dir(dir_path);
  }
  FILE *output_file = fopen(args.ofilepath, "a");
  if (output_file == NULL) {
    ERROR("cannot open file %s %s\n", args.ofilepath, strerror(errno));
    exit(1);
  }

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
    } else {
      size_unit_str = "B";
    }
  }

  printf("\n");
  for (int i = 0; i < args.n_cache_size * args.n_eviction_algo; i++) {
    snprintf(output_str, 1024,
             "%s %s cache size %8ld%s, %lld req, miss ratio %.4lf, byte miss "
             "ratio %.4lf\n",
             args.reader->trace_path, result[i].cache_name,
             (long)(result[i].cache_size / size_unit), size_unit_str,
             (long long)result[i].n_req,
             (double)result[i].n_miss / (double)result[i].n_req,
             (double)result[i].n_miss_byte / (double)result[i].n_req_byte);
    printf("%s", output_str);
    fprintf(output_file, "%s", output_str);
  }
  fclose(output_file);

  if (args.n_cache_size * args.n_eviction_algo > 0)
    my_free(sizeof(cache_stat_t) * args.n_cache_size * args.n_eviction_algo, result);

  free_arg(&args);

  return 0;
}

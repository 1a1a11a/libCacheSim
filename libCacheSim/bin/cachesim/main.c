

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/simulator.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

int main(int argc, char **argv) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  if (args.n_cache_size == 1) {
    simulate(args.reader, args.cache, args.warmup_sec, args.ofilepath);
    return 0;
  }

  cache_stat_t *result = simulate_at_multi_sizes(
      args.reader, args.cache, args.n_cache_size, args.cache_sizes, NULL, 0,
      args.warmup_sec, args.n_thread);

  char output_str[1024];
  char output_filename[128];
  create_dir("result/");
  sprintf(output_filename, "result/%s", rindex(args.trace_path, '/') + 1);
  FILE *output_file = fopen(output_filename, "a");
  for (int i = 0; i < args.n_cache_size; i++) {
    snprintf(output_str, 1024,
             "%s %s, cache size %16" PRIu64 "MiB : miss/n_req %16" PRIu64
             "/%16" PRIu64
             " (%.4lf), "
             "byte miss ratio %.4lf\n",
             output_filename, args.cache->cache_name,
             result[i].cache_size / (uint64_t)MiB, result[i].n_miss,
             result[i].n_req,
             (double)result[i].n_miss / (double)result[i].n_req,
             (double)result[i].n_miss_byte / (double)result[i].n_req_byte);
    printf("%s", output_str);
    fprintf(output_file, "%s", output_str);
  }
  fclose(output_file);

  close_reader(args.reader);
  args.cache->cache_free(args.cache);

  return 0;
}

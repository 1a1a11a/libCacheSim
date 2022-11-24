

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
  if (args.cache_sizes[0] > GiB) {
    size_unit = GiB;
    size_unit_str = "GB";
  } else if (args.cache_sizes[0] > MiB) {
    size_unit = MiB;
    size_unit_str = "MB";
  } else if (args.cache_sizes[0] > KiB) {
    size_unit = KiB;
    size_unit_str = "KB";
  }

  for (int i = 0; i < args.n_cache_size; i++) {
    snprintf(output_str, 1024,
             "%s %s, cache size %16" PRIu64 "%s : miss/n_req %16" PRIu64
             "/%16" PRIu64
             " (%.4lf), "
             "byte miss ratio %.4lf\n",
             output_filename, args.cache->cache_name,
             result[i].cache_size / size_unit, size_unit_str, result[i].n_miss,
             result[i].n_req,
             (double)result[i].n_miss / (double)result[i].n_req,
             (double)result[i].n_miss_byte / (double)result[i].n_req_byte);
    printf("%s", output_str);
    fprintf(output_file, "%s", output_str);
  }
  fclose(output_file);

#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
  sprintf(output_filename, "result/%s.evictionAge",
          rindex(args.trace_path, '/') + 1);
  request_t *req = new_request();
  reader_set_read_pos(args.reader, 1.0);
  read_one_req_above(args.reader, req);

  for (int i = 0; i < args.n_cache_size; i++) {
    dump_eviction_age(caches[i], output_filename);
    dump_cached_obj_age(caches[i], req, output_filename);
  }

  free_request(req);
#endif

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




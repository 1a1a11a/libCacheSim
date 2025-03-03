#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>
#include <libgen.h>
#include <unistd.h>
#include "../../include/libCacheSim/sampling.h"
#include "../cachesim/internal.h"
#include "mrc_internal.h"

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "Usage:\n"
            "  For SHARDS:\n"
            "    %s SHARDS <output_file> <trace_file> <trace_type> <rate> [--size SIZE] [other options]\n\n"
            "  For MINI:\n"
            "    %s MINI <trace_file> <trace_type> <eviction_algo> <cache_sizes> <rate> <output_file> [other options]\n",
            argv[0], argv[0]);
    return 1;
}

  // printf("Received Arguments:\n");
  // for (int i = 0; i < argc; i++) {
  //   printf("argv[%d]: %s\n", i, argv[i]);
  // }

  char *algorithm_type = argv[1];
  printf("Algorithm type: %s\n", algorithm_type);
  if (strcmp(algorithm_type, "MINI") == 0) {
    char* path=argv[7];
    struct MINI_arguments arguments;
    parse_mini_cmd(argc, argv, &arguments);
    cache_stat_t *return_value = generate_mini_mrc(&arguments);

    FILE *output_file = fopen(path, "w");
    if (output_file == NULL) {
      perror("Error opening file");
      return 1;
    }

    fprintf(output_file, "Cache Size,Miss Ratio, Miss Ratio Byte\n");
    for (int i = 0; i < arguments.n_cache_size * arguments.n_eviction_algo; i++) {
      uint64_t cache_size = (uint64_t)((float)return_value[i].cache_size / return_value[i].sampler_ratio);
      double miss_ratio = (double)return_value[i].n_miss / (double)return_value[i].n_req;
      double miss_ratio_byte = (double)return_value[i].n_miss_byte / (double)return_value[i].n_req_byte;
      fprintf(output_file, "%ld,%f, %f\n", cache_size, miss_ratio, miss_ratio_byte);
    }

    fclose(output_file);
  } else if (strcmp(algorithm_type, "SHARDS") == 0) {
    struct PARAM params;
    char *path = argv[2];
    parse_mrc_cmd(argc, argv, &params);

    params.mrc_algo(&params, path);
  } else {
    fprintf(stderr, "Error: unknown algorithm type\n");
    return 1;
  }
}
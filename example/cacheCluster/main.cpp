
#include <inttypes.h>

#include <iostream>
#include <string>
#include <vector>

#include "include/cache.hpp"
#include "include/cacheCluster.hpp"
#include "include/cacheServer.hpp"
#include "libCacheSim.h"

namespace CDNSimulator {

void simulate(int argc, char *argv[]) {
  const char *data_path = "../../../data/twitter_cluster52.csv";
  if (argc > 1) {
    data_path = argv[1];
  } else {
    printf("use default data at ../../../data/twitter_cluster52.csv\n");
  }

  if (access(data_path, F_OK) == -1) {
    printf("Data file %s does not exist.\n", data_path);
    exit(1);
  }

  /* setup a csv reader */
  reader_init_param_t init_params = default_reader_init_params();
  init_params.obj_id_field = 2;
  init_params.obj_size_field = 3;
  init_params.time_field = 1;
  init_params.has_header_set = true;
  init_params.has_header = true;
  init_params.delimiter = ',';

  /* we can also use open_trace with the same parameters */
  reader_t *reader = open_trace(data_path, CSV_TRACE, &init_params);

  const uint64_t n_server = 10;
  const uint64_t server_dram_cache_size = 1 * MiB;
  const uint64_t server_disk_cache_size = 10 * MiB;
  const std::string algo = "lru";
  // each cache holds 2 ** 20 objects, this is for performance optimization
  // you can specify a smaller number to save memory
  const uint32_t hashpower = 20;
  printf(
      "setting up a cluster of %lu servers, each server has %lu MB DRAM cache "
      "and %lu MB disk cache, using %s as cache replacement algorithm\n",
      (unsigned long)n_server, (unsigned long)(server_dram_cache_size / MiB),
      (unsigned long)(server_disk_cache_size / MiB), algo.c_str());

  CacheCluster cluster(0);

  for (int i = 0; i < n_server; i++) {
    CacheServer server(i);
    server.add_cache(std::move(Cache(server_dram_cache_size, algo, hashpower)));
    server.add_cache(std::move(Cache(server_disk_cache_size, algo, hashpower)));
    cluster.add_server(std::move(server));
  }

  /* read the trace */
  request_t *req = new_request();

  uint64_t n_miss = 0, n_miss_byte = 0;
  uint64_t n_req = 0, n_req_byte = 0;

  while (read_trace(reader, req) == 0) {
    bool hit = cluster.get(req);
    if (!hit) {
      n_miss += 1;
      n_miss_byte += req->obj_size;
    }
    n_req += 1;
    n_req_byte += req->obj_size;
  }

  std::cout << n_req << " requests, " << n_miss << " misses, miss ratio: " << (double)n_miss / n_req
            << ", byte miss ratio: " << (double)n_miss_byte / n_req_byte << std::endl;

  close_trace(reader);
}
}  // namespace CDNSimulator

int main(int argc, char *argv[]) {
  try {
    CDNSimulator::simulate(argc, argv);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    print_stack_trace();
  }

  return 0;
}

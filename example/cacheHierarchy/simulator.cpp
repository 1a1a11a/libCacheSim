//
// Created by Juncheng Yang on 11/15/19.
//

#include "simulator.hpp"

#include "libCacheSim/cache.h"
#include "libCacheSim/plugin.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/simulator.h"

using namespace std;

double Simulator::gen_miss_trace(string algo, uint64_t cache_size,
                                 string trace_path, string miss_output_path) {
  uint64_t n_req = 0, n_hit = 0;
  std::ofstream miss_ofs(miss_output_path);

  reader_init_param_t reader_init_params = {
      .time_field = 1, .obj_id_field = 2, .obj_size_field = 3, .next_access_vtime_field = 4};
  // see the cacheSimulator example for using csv trace
  reader_init_params.binary_fmt_str = "<IQIQ";
  reader_t *reader =
      open_trace(trace_path.c_str(), BIN_TRACE, &reader_init_params);
  common_cache_params_t cc_params = {.cache_size = cache_size};
  cache_t *cache = create_cache(algo.c_str(), cc_params, nullptr);

  request_t *req = new_request();

  bool hit;
  read_one_req(reader, req);
  while (req->valid) {
    n_req += 1;
    hit = cache->get(cache, req);
    if (!hit)
      miss_ofs << req->clock_time << "," << (uint64_t)req->obj_id << ","
               << req->obj_size << std::endl;
    else
      n_hit += 1;
    read_one_req(reader, req);
  }

  miss_ofs.close();
  std::cout << trace_path << ", object miss ratio "
            << 1.0 - (double)n_hit / n_req << std::endl;

  return 1.0 - (double)n_hit / n_req;
}

void Simulator::output_mrc(string &algo, vector<uint64_t> cache_sizes,
                           string &trace_path, string &mrc_output_path) {
  char alg[] = "LRU";
  reader_init_param_t reader_init_params = {
      .time_field = 1, .obj_id_field = 2, .obj_size_field = 3};
  reader_init_params.binary_fmt_str = "<III";
  reader_t *reader =
      open_trace(trace_path.c_str(), BIN_TRACE, &reader_init_params);

  common_cache_params_t ccparams = {.cache_size = cache_sizes[0]};
  cache_t *cache = create_cache(algo.c_str(), ccparams, nullptr);

  uint64_t cache_size_array[cache_sizes.size()];
  std::copy(cache_sizes.begin(), cache_sizes.end(), cache_size_array);

  auto mrc = simulate_at_multi_sizes(reader, cache, cache_sizes.size(),
                                     cache_size_array, nullptr, 0, 0,
                                     std::thread::hardware_concurrency(), false);

  std::ofstream mrc_ofs(mrc_output_path);
  mrc_ofs << "# L2, " << mrc[0].n_req << " req, " << mrc[0].n_req_byte
          << " byte" << std::endl;
  mrc_ofs << "# cache size, miss_cnt, miss_byte" << std::endl;
  for (int i = 0; i < cache_sizes.size(); i++) {
    mrc_ofs << mrc[i].cache_size << "," << mrc[i].n_miss << ","
            << mrc[i].n_miss_byte << std::endl;
  }

  mrc_ofs.close();
}

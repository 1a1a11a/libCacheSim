//
// Created by Juncheng Yang on 4/20/20.
//

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "myconfig.hpp"
#include "simulator.hpp"
#include "utils.hpp"

using namespace std;

int main(int argc, char* argv[]) {
  string cache_algo = "LRU";

  if (argc < 2) {
    std::cout << "usage: ./" << argv[0] << " path/to/config/file" << std::endl;
    return 0;
  }

  // load config
  string config_path = string(argv[1]);
  Myconfig config(config_path);
  config.print();

  // start L1: generate N cachemiss traces
  vector<thread> simulator_thread_vec;
  for (int i = 0; i < config.n_l1; i++) {
    uint64_t cache_size = config.l1_sizes.at(i);
    string& trace_path = config.l1_trace_path.at(i);
    string& miss_output_path = config.l1_miss_output_path.at(i);
    std::thread t(Simulator::gen_miss_trace, cache_algo, cache_size, trace_path,
                  miss_output_path);
    simulator_thread_vec.push_back(std::move(t));
  }

  for (auto& t : simulator_thread_vec) {
    t.join();
  }

  // create L2 traces
  std::cout << "************* create L2 trace "
            << TraceUtils::merge_l1_trace(config.l1_miss_output_path,
                                          config.l2_trace_path)
            << " req" << std::endl;

  // start L2: get a MRC
  Simulator::output_mrc(cache_algo, config.l2_sizes, config.l2_trace_path,
                        config.l2_mrc_output_path);

  return 0;
}
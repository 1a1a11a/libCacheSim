//
// Created by Juncheng Yang on 11/15/19.
//

#ifndef libCacheSim_CACHESIMULATOR_HPP
#define libCacheSim_CACHESIMULATOR_HPP

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

class Simulator {
 public:
  /* return obj miss ratio */
  static double gen_miss_trace(string algo, uint64_t cache_size,
                               string trace_path, string miss_output_path);

  static void output_mrc(string &algo, std::vector<uint64_t> cache_sizes,
                         string &trace_path, string &mrc_output_path);
};

#endif  // libCacheSim_CACHESIMULATOR_H

//
// Created by Juncheng Yang on 4/21/20.
//

#ifndef CACHESIMULATORCPP_CONFIG_H
#define CACHESIMULATORCPP_CONFIG_H

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "utils.hpp"
#include "fkYAML/node.hpp"

using namespace std;

class Myconfig {
 private:
 public:
  std::string config_path;
  std::string output_path;

  uint64_t n_l1 = 0;
  std::vector<string> l1_sizes_str;
  std::vector<uint64_t> l1_sizes;  // a list of size for each L1
  std::vector<std::string> l1_trace_path;

  std::vector<string> l2_sizes_str;
  std::vector<uint64_t> l2_sizes;  // L2 size to evaluate

  std::vector<std::string> l1_names;
  std::vector<std::string> l1_miss_output_path;
  std::string l2_trace_path;
  std::string l2_mrc_output_path;

  explicit Myconfig(std::string& path) : config_path(path) {
    load_config();
    _prepare();
  };

  ~Myconfig() = default;

  void load_config();

  void _prepare();

  void print() {
    std::cout << "************************* Config *************************"
              << std::endl;
    std::cout << n_l1 << " L1 caches" << std::endl;
    for (int i = 0; i < l1_trace_path.size(); i++) {
      std::cout << "size " << l1_sizes_str.at(i) << ", trace "
                << l1_trace_path.at(i) << std::endl;
    }
    std::cout << "L2 evaluate sizes ";
    for (auto& sz : l2_sizes_str) std::cout << sz << ",";
    std::cout << ", output " << l2_trace_path << std::endl;
    std::cout << "************************* simulation start "
                 "*************************"
              << std::endl;
  }
};

#endif  // CACHESIMULATORCPP_CONFIG_H

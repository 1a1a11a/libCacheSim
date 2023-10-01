//
// Created by Juncheng Yang on 4/21/20.
//

#include "myconfig.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <cassert>
#include <iostream>

using namespace std;

void Myconfig::load_config() {
  YAML::Node yamlconfig = YAML::LoadFile(config_path);
  assert(yamlconfig["L1"]["path"].IsSequence());
  assert(yamlconfig["L2"]["size"].IsSequence());

  n_l1 = yamlconfig["L1"]["path"].size();
  uint64_t l1_size =
      Utils::convert_size_str(yamlconfig["L1"]["size"].as<string>());
  for (int i = 0; i < n_l1; i++) {
    l1_sizes.push_back(l1_size);
    l1_sizes_str.push_back(yamlconfig["L1"]["size"].as<string>());
  }

  for (std::size_t i = 0; i < n_l1; i++) {
    l1_trace_path.push_back(yamlconfig["L1"]["path"][i].as<string>());
  }

  for (std::size_t i = 0; i < yamlconfig["L2"]["size"].size(); i++) {
    string sz = yamlconfig["L2"]["size"][i].as<string>();
    l2_sizes.push_back(Utils::convert_size_str(sz));
    l2_sizes_str.push_back(sz);
  }

  output_path = yamlconfig["output"].as<string>();
}

void Myconfig::_prepare() {
  for (int i = 0; i < n_l1; i++) {
    int pos = l1_trace_path.at(i).rfind('/');
    if (pos == std::string::npos) pos = -1;
    l1_names.push_back(l1_trace_path.at(i).substr(pos + 1));

    l1_miss_output_path.push_back(output_path + "/l1_trace_" + l1_names.back());
  }
  l2_trace_path = output_path + "/l2.trace";
  l2_mrc_output_path = output_path + "/l2.mrc";
  mkdir(output_path.c_str(), 0777);
}

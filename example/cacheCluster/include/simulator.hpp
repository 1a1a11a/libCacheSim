//
//  simulator.cpp
//  CDNSimulator
//
//  Created by Juncheng on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP


#include "boost/program_options.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <future>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include "cacheCluster.hpp"
#include "cacheClusterThread.hpp"
#include "cacheServer.hpp"
#include "constCDNSimulator.hpp"


#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "cache.h"
#include "cacheHeader.h"
#include "csvReader.h"
#include "reader.h"

#ifdef __cplusplus
}
#endif


namespace CDNSimulator {

  typedef struct {
    std::string param_string;
    std::string trace_path;
    std::string trace_type;
    std::string cache_alg;
    std::string exp_name;
    unsigned char reader_type;
    unsigned char data_type;
    void *reader_init_params;
    unsigned long n_server;
    unsigned long server_cache_size;
    unsigned long *server_cache_sizes;
    std::string log_folder;
    unsigned long log_interval;
    bool ICP;
    bool ghost_parity;
    unsigned int EC_n;
    unsigned int EC_k;
    unsigned int EC_x;
    unsigned long EC_size_threshold;
    unsigned int admission_limit; // only cache an object after being requested
    // admission_limit times
    std::string failure_data; // this should be a file, each line of which state
    // the failed server for the next 5 min
  } cache_cluster_arg_t;

  typedef cache_cluster_arg_t simulator_arg_t;

  void simulate(cache_cluster_arg_t *cc_arg);

  long convert_size(std::string str_size) {
    if (std::tolower(str_size.back()) == 'm') {
      str_size.pop_back();
      return std::stol(str_size) * 1000 * 1000;
    } else if (std::tolower(str_size.back()) == 'g') {
      str_size.pop_back();
      return std::stol(str_size) * 1000 * 1000 * 1000;
    } else if (std::tolower(str_size.back()) == 't') {
      str_size.pop_back();
      return std::stol(str_size) * 1000 * 1000 * 1000 * 1000;
    } else {
      return std::stol(str_size);
    }
  }

  void parse_cmd_arg2(int argc, char *argv[],
                      CDNSimulator::cache_cluster_arg_t *sargs) {

    const time_t t = time(0);
    tm *ltm = localtime(&t);

    sargs->EC_n = 0;
    sargs->EC_k = 0;
    sargs->EC_size_threshold = 0;
    sargs->admission_limit = 0;
    sargs->log_interval = 300;
    sargs->ghost_parity = false;
    sargs->ICP = false;
    sargs->log_folder = "log_" + std::to_string(ltm->tm_mon + 1) + "_" +
                        std::to_string(ltm->tm_mday);
    mkdir(sargs->log_folder.c_str(), 0770);

    std::string trace_type;
    std::string server_cache_size;
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help,h", "Print help messages")(
      "name", po::value<std::string>(&(sargs->exp_name))->default_value("cluster"),
      "exp name")(
      "alg,a", po::value<std::string>(&(sargs->cache_alg)),
      "cache replacement algorithm")(
      "dataPath,d", po::value<std::string>(&(sargs->trace_path))->required(),
      "data path")(
      "serverCacheSize,s", po::value<std::string>(&server_cache_size)->required(),
      "per server cache size (avg if differnet)")(
      "serverCacheSizes", po::value<std::vector<std::string>>()->multitoken(),
      "list of cache sizes")(
      "nServer,m", po::value<unsigned long>(&(sargs->n_server))->required(),
      "the number of cache servers in the cluster")(
      "traceType,t", po::value<std::string>(&(sargs->trace_type))->required(),
      "the type of trace[akamai1b/wiki]")(
      "logInterval,l", po::value<unsigned long>(&(sargs->log_interval)),
      "the log output interval in virtual time")(
      "EC_n,n", po::value<unsigned int>(&(sargs->EC_n))->required(), "N in EC")(
      "EC_k,k", po::value<unsigned int>(&(sargs->EC_k))->required(),
      "K in EC")("EC_x,x", po::value<unsigned int>(&(sargs->EC_x))->required(),
                 "number of extra fetch - deprecated")(
      "admission,o",
      po::value<unsigned int>(&(sargs->admission_limit))->default_value(0),
      "n-hit-winder filters")(
      "EC_sizeThreshold,z",
      po::value<unsigned long>(&(sargs->EC_size_threshold))->default_value(0),
      "size threshold for coding")("ghostParity,g",
                                   po::value<bool>(&(sargs->ghost_parity)),
                                   "whether send out ghost parity")(
      "ICP,i", po::value<bool>(&(sargs->ICP)), "whether enable ICP")(
      "failureData,f",
      po::value<std::string>(&(sargs->failure_data))->default_value(""),
      "Path to failure data");
    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);
      }
      po::notify(vm); // throws on error, so do after help in case

      sargs->server_cache_size = convert_size(server_cache_size);
      sargs->server_cache_sizes = new unsigned long[sargs->n_server];

      if (!vm["serverCacheSizes"].empty()) {
        std::vector<std::string> cache_sizes;
        cache_sizes = vm["serverCacheSizes"].as<std::vector<std::string>>();

        if (cache_sizes.size() == 1) {
          std::vector<std::string> cache_sizes_new;
          boost::split(cache_sizes_new, cache_sizes.at(0), [](char c) { return c == ' '; });
          cache_sizes = cache_sizes_new;
        }
        DEBUG("%lu server cache sizes, first one %s\n", cache_sizes.size(), cache_sizes[0].c_str())

        long sum_cache_size = 0;
        for (unsigned long i = 0; i < sargs->n_server; i++) {
          sum_cache_size += convert_size(cache_sizes.at(i));
          sargs->server_cache_sizes[i] = convert_size(cache_sizes.at(i));
        }
        if (std::abs(sum_cache_size - (long) (sargs->server_cache_size * sargs->n_server)) > 1000) {
          std::cerr << "sum of given cache sizes " << sum_cache_size / 1000 / 1000 / 1000 << "GB (" << sum_cache_size
                    << ")" << " is not the same as avg cache size " << sargs->server_cache_size / 1000 / 1000 / 1000
                    << "GB - " << server_cache_size << "* n_server (" << sargs->n_server << ")" << std::endl;
          exit(0);
        }
      } else {
        INFO("fill all servers with same cache size %ld GB\n", sargs->server_cache_size/1000/1000/1000);
        std::fill_n(sargs->server_cache_sizes, sargs->n_server,
                    sargs->server_cache_size);
      }
    } catch (po::error &e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cerr << desc << std::endl;
      exit(1);
    }

    if (sargs->trace_type == "akamai1b") {
      std::string fmt = "III";
      sargs->reader_type = BIN_TRACE;
      sargs->data_type = OBJ_ID_NUM;
      sargs->reader_init_params =
        (void *) new_binaryReader_init_params(2, -1, 1, 3, fmt.c_str());
    } else if (sargs->trace_type == "akamai1bWithHost") {
      std::string fmt = "IIII";
      sargs->reader_type = BIN_TRACE;
      sargs->data_type = OBJ_ID_NUM;
      sargs->reader_init_params =
        (void *) new_binaryReader_init_params(2, 4, 1, 3, fmt.c_str());
    } else if (sargs->trace_type == "akamai1bWithBucket") {
      std::string fmt = "IIII";
      sargs->reader_type = BIN_TRACE;
      sargs->data_type = OBJ_ID_NUM;
      sargs->reader_init_params =
        (void *) new_binaryReader_init_params(2, 4, 1, 3, fmt.c_str());
    } else if (sargs->trace_type == "simpleBinary") {
      std::string fmt = "I";
      sargs->reader_type = BIN_TRACE;
      sargs->data_type = OBJ_ID_NUM;
      sargs->reader_init_params =
        (void *) new_binaryReader_init_params(1, -1, -1, -1, fmt.c_str());
    } else if (sargs->trace_type == "wiki") {
      sargs->data_type = 'c';
      sargs->reader_type = CSV;
      sargs->reader_init_params = (void *) WIKI_CSV_PARAM_INIT;
    } else if (sargs->trace_type == "wiki2") {
      sargs->data_type = OBJ_ID_STR;
      sargs->reader_type = CSV_TRACE;
      sargs->reader_init_params = (void *) WIKI2_CSV_PARAM_INIT;
    } else if (sargs->trace_type == "wiki2b") {
      std::string fmt = "Li";
      sargs->reader_type = BIN_TRACE;
      sargs->data_type = OBJ_ID_NUM;
      sargs->reader_init_params =
        (void *) new_binaryReader_init_params(1, -1, -1, 2, fmt.c_str());
    } else {
      ERROR("unknown trace type %s\n", optarg);
      abort();
    }

    INFO("trace: %s, cache alg %s, cache_size %lu, n_server %lu, trace_type %s, "
         "EC_n %d, EC_k %d, EC_x %u, "
         "admission_limit %u, EC_size_threshold %lu, ghost_parity %d, ICP %d, "
         "failure_data %s\n",
         sargs->trace_path.c_str(), sargs->cache_alg.c_str(),
         sargs->server_cache_size, sargs->n_server, sargs->trace_type.c_str(),
         sargs->EC_n, sargs->EC_k, sargs->EC_x, sargs->admission_limit,
         sargs->EC_size_threshold, sargs->ghost_parity, sargs->ICP,
         sargs->failure_data.c_str());
  } // namespace CDNSimulator

  std::vector<std::string> get_traces_from_config(const std::string &config_loc) {

    std::ifstream ifs(config_loc, std::ios::in);
    std::vector<std::string> traces;
    std::string line;
    while (getline(ifs, line)) {
      traces.push_back(line);
    }

    ifs.close();
    return traces;
  }

} // namespace CDNSimulator

#endif /* SIMULATOR_HPP */

//
//  simulator.cpp
//  CDNSimulator
//
//  Created by Juncheng on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "simulator.hpp"


namespace CDNSimulator {

void simulate(cache_cluster_arg_t *cc_arg) {

  reader_t *reader =
      setup_reader(cc_arg->trace_path.c_str(), cc_arg->reader_type,
                   cc_arg->data_type, 0, 0, (void *) cc_arg->reader_init_params);

  /* threads for cacheServerThread and cacheClusterThread */
  std::thread *t;
  std::vector<std::thread *> threads;
  cacheServer *cache_servers[cc_arg->n_server];
  cacheCluster *cache_cluster;
  cacheClusterThread *cache_cluster_thread;

  try {
    /* initialize cacheServers */
    if (cc_arg->cache_alg.find("size") == std::string::npos) {
      assert(cc_arg->EC_size_threshold == 0);
    }

    std::string exp_name = cc_arg->log_folder + "/" +
            cc_arg->exp_name + "_" +
            cc_arg->cache_alg + "_" +
            std::to_string(cc_arg->server_cache_size*cc_arg->n_server/1000000000) + "GB" + "_o" +
            std::to_string(cc_arg->admission_limit) + "_n" +
            std::to_string(cc_arg->EC_n) + "_k" +
            std::to_string(cc_arg->EC_k) + "_z" +
            std::to_string(cc_arg->EC_size_threshold);


    for (unsigned long i = 0; i < cc_arg->n_server; i++) {
      cache_servers[i] = new cacheServer(
          i + 1, cc_arg->server_cache_sizes[i], cc_arg->cache_alg,
          cc_arg->data_type,
          (void*)(exp_name.c_str()),
          "server_" + std::to_string(i + 1),
          cc_arg->EC_n, cc_arg->EC_k);
    }

    std::vector<std::string> split_results;
    boost::split(split_results, cc_arg->trace_path, [](char c){return c == '/';});

    /* initialize cacheCluster and cacheClusterThread */
    cache_cluster =
        new cacheCluster(1, cc_arg->exp_name, split_results.back(), cache_servers, cc_arg->n_server, cc_arg->cache_alg,
                         cc_arg->server_cache_sizes, cc_arg->admission_limit,
                         cc_arg->ICP, cc_arg->EC_n, cc_arg->EC_k, cc_arg->EC_x,
                         cc_arg->ghost_parity, cc_arg->EC_size_threshold, cc_arg->trace_type);
    cache_cluster_thread =
        new cacheClusterThread(cc_arg->param_string, cache_cluster, reader, cc_arg->failure_data,
                               cc_arg->log_folder, cc_arg->log_interval);

    /* build thread for cacheClusterThread */
    t = new std::thread(&cacheClusterThread::run, cache_cluster_thread);
    threads.push_back(t);

    INFO("main thread finishes initialization, begins waiting for all cache "
         "clusters\n");
    /* wait for cache server and cache layer to finish computation */
    for (auto it = threads.begin(); it < threads.end(); it++) {
      (**it).join();
      delete *it;
    }
    INFO("all cache servers and clusters joined\n");

    /* free cacheServerThread and cacheClusterThread */
    if (cc_arg->cache_alg.find("size") != std::string::npos) {
      for (auto cache_server : cache_servers) {
        std::cout << "final server used size (GB): "
                  << cache_server->cache->used_size/1000/1000/1000 << "/"
                  << cache_server->cache->size/1000/1000/1000 << std::endl;
        delete cache_server;
      }
    }
    delete cache_cluster;
    delete cache_cluster_thread;

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    print_stack_trace();
  }
  close_reader(reader);
}
} // namespace CDNSimulator



int simulator(int argc, char *argv[]) {

  CDNSimulator::simulator_arg_t arg;
  parse_cmd_arg2(argc, argv, &arg);

  for (long i = 1; i < argc; i++)
    arg.param_string += std::string(argv[i]) + " ";

  // Jason: assuming equal size and equal weight
  CDNSimulator::simulate(&arg);

  g_free(arg.reader_init_params);
  return 1;
}

int main(int argc, char *argv[]) {

  try {
    simulator(argc, argv);
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    print_stack_trace();
  }

  return 0;
}

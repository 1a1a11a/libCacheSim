//
// Created by Xiaojun Guo on 25/2/25.
//

#include "../cli_reader_utils.h"
#include "internal.h"
#include "traceAnalyzer/analyzer.h"

using namespace traceAnalyzer;

void print_args(struct arguments *args) {
  printf("args: \n");
  for (int i = 0; i < N_ARGS; i++) {
    printf("%s\n", args->args[i]);
  }
  printf("trace_path: %s\n", args->trace_path);
  printf("trace_type: %d\n", args->trace_type);
  printf("trace_type_params: %s\n", args->trace_type_params);
  printf("ofilepath: %s\n", args->ofilepath);
  printf("n_req: %lld\n", (long long)args->n_req);
  printf("verbose: %d\n", args->verbose);
  printf("cache_algorithm_str: %s\n", args->cache_algorithm_str);
  printf("mrc_size_str: %s\n", args->mrc_size_str);
  printf("profiler_str: %s\n", args->mrc_profiler_str);
  printf("mrc_profiler_params_str: %s\n", args->mrc_profiler_params_str);

  for (size_t i = 0; i < args->mrc_profiler_params.profile_size.size(); i++) {
    printf("profile_size: %ld\n", args->mrc_profiler_params.profile_size[i]);
  }
  printf("====\n");
  for (size_t i = 0; i < args->mrc_profiler_params.profile_wss_ratio.size();
       i++) {
    printf("profile_wss_ratio: %f\n",
           args->mrc_profiler_params.profile_wss_ratio[i]);
  }

  args->mrc_profiler_params.shards_params.print();
  args->mrc_profiler_params.minisim_params.print();
}

int main(int argc, char *argv[]) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  mrcProfiler::MRCProfilerBase *profiler =
      create_mrc_profiler(args.mrc_profiler_type, args.reader, args.ofilepath,
                          args.mrc_profiler_params);

  profiler->run();

  profiler->print(args.ofilepath);

  delete profiler;

  close_reader(args.reader);

  return 0;
}

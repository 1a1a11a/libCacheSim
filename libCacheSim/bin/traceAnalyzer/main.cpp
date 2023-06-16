//
// Created by Juncheng Yang on 5/9/21.
//

#include "../../traceAnalyzer/analyzer.h"
#include "../cli_reader_utils.h"
#include "internal.h"

using namespace traceAnalyzer;

int main(int argc, char *argv[]) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  TraceAnalyzer *stat = new TraceAnalyzer(
      args.reader, args.ofilepath, args.analysis_option, args.analysis_param);
  stat->run();

  // } else if (strcasecmp(args.task, "hotOS23") == 0) {
  //   args.analysis_option.popularity_decay = true;
  //   args.analysis_option.prob_at_age = true;
  //   args.analysis_option.lifetime = false;

  //   args.analysis_param.time_window = 300;

  ofstream ofs("traceStat", ios::out | ios::app);
  ofs << *stat << endl;
  ofs.close();
  cout << *stat;

  delete stat;

  close_reader(args.reader);

  return 0;
}
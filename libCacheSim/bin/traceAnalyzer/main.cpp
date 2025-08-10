//
// Created by Juncheng Yang on 5/9/21.
//

#include "../cli_reader_utils.h"
#include "internal.h"
#include "traceAnalyzer/analyzer.h"

using namespace traceAnalyzer;

int main(int argc, char *argv[]) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  TraceAnalyzer *stat = new TraceAnalyzer(
      args.reader, args.ofilepath, args.analysis_option, args.analysis_param);
  stat->run();

  ofstream ofs("traceStat", ios::out | ios::app);
  ofs << *stat << endl;
  ofs.close();
  cout << *stat;

  delete stat;

  close_reader(args.reader);

  return 0;
}

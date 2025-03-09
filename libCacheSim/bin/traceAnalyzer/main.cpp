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

  string csv_path = "test.csv";

  TraceAnalyzer *stat = new TraceAnalyzer(
      args.reader, args.ofilepath, args.analysis_option, args.analysis_param);
  stat->run();

  // trace, n_req, n_obj, n_req_GiB, n_obj_GiB, compulsory_miss_ratio, compulsory_miss_ratio_byte, obj_size_weighted_by_req_per_obj, freq_mean, time_span,
  // n_op_nop, n_op_get, n_op_gets, n_op_set, n_op_add, n_op_cas, n_op_replace, n_op_append, n_op_prepend, n_op_delete, n_op_incr,
  // n_op_decr, n_op_read, n_op_write, n_op_update, n_op_invalid, n_write, n_overwrite, n_del,
  // req_rate_min, req_rate_max, req_rate_window, obj_rate_min, obj_rate_max, obj_rate_window,
  // popularity_slope, popularity_intercept, popularity_r2, x-hit, freq_of_most_popular_obj
  ofstream ofs(csv_path, ios::out | ios::app);
  ofs << *stat;
  ofs.close();
  // cout << *stat;

  delete stat;

  close_reader(args.reader);

  return 0;
}
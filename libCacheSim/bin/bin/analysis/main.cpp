//
// Created by Juncheng Yang on 5/9/21.
//

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <iostream>

#include "../../analysis/analysis.h"
#include "../utils.h"
#include "params.h"

using namespace std;
using namespace analysis;
namespace po = boost::program_options;

int main(int argc, char *argv[]) {
  string task;
  string param_string = "";
  // setenv("TCMALLOC_LARGE_ALLOC_REPORT_THRESHOLD", "10000000000", 1);

  po::options_description desc("Analysis options");
  desc.add_options()("task", po::value<std::string>(&task)->required(),
                     "task to be performed "
                     "[basic/all/popularity/popularityDecay/reuse/size/reqRate/"
                     "accessPattern/createFutureReuse/probAtAge]")(
      "params,x", po::value<std::string>(&param_string), "analysis parameters");

  cli::cli_arg_t cli_arg = cli::Util::parse_cli_arg(argc, argv, desc);

  std::cout << "input traces " << cli_arg.ipath << ", type "
            << cli_arg.trace_type << std::endl;

  reader_t *reader = cli::Util::create_reader(&cli_arg);

  struct analysis_option analysis_option = default_option();
  struct analysis_param analysis_param = parse_param(param_string);

  Analysis *stat = nullptr;

  if (boost::iequals(task, "basic") || boost::iequals(task, "stat")) {
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "all")) {
    analysis_option.req_rate = true;
    analysis_option.access_pattern = true;
    analysis_option.size = true;
    analysis_option.reuse = true;
    analysis_option.popularity = true;
    analysis_option.popularity_decay = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "popularity")) {
    analysis_option.popularity = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "popularityDecay")) {
    analysis_option.popularity_decay = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "reuse")) {
    analysis_option.reuse = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "size")) {
    analysis_option.size = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "reqRate")) {
    analysis_option.req_rate = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "accessPattern")) {
    analysis_option.access_pattern = true;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else if (boost::iequals(task, "hotOS23")) {
    analysis_option.popularity_decay = true;
    analysis_option.prob_at_age = true;
    analysis_option.lifetime = false;

    analysis_param.time_window = 300;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
    delete stat;

    reset_reader(reader);
    analysis_param.time_window = 60;
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();

  } else if (boost::iequals(task, "test")) {
    stat = new Analysis(reader, cli_arg.opath, analysis_option, analysis_param);
    stat->run();
  } else {
    std::cerr << "unknown task " << task << "\n";
    exit(1);
  }

  if (boost::iequals(task, "basic") || boost::iequals(task, "all")) {
    ofstream ofs("traceStat", ios::out | ios::app);
    ofs << *stat << endl;
    ofs.close();
    cout << *stat;
  }

  delete stat;

  return 0;
}

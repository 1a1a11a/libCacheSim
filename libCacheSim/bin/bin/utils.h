

#pragma once

#include <strings.h>

#include <boost/program_options.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>
#include <thread>

#include "../include/logging.h"
#include "../traceReader/reader.h"

using namespace std;

namespace cli {
namespace po = boost::program_options;

#define MAX_CACHE_SIZES 128

struct cli_arg {
  std::string ipath;
  std::string opath;
  std::string trace_type;

  long cache_sizes[MAX_CACHE_SIZES];
  int n_thread{-1}; /* -1 means dynamic, if there are still capacity left, we
                       will use it */
};

typedef struct cli_arg cli_arg_t;

class Util {
 public:
  static void detect_trace_type(cli_arg_t *arg) {
    const char *trace_path = arg->ipath.c_str();
    if (strcasestr(arg->ipath.c_str(), "oracleGeneralBin") != NULL ||
        strcasestr(arg->ipath.c_str(), "oracleGeneral.") != NULL) {
      arg->trace_type = "oracleGeneral";
    } else if (strcasestr(arg->ipath.c_str(), "oracleSysTwrNS") != NULL) {
      arg->trace_type = "oracleSysTwrNS";
    } else {
      ;
    }

    if (strcasestr(trace_path, "oracleGeneralBin") != NULL ||
        strcasestr(trace_path, "oracleGeneral.bin") != NULL ||
        strcasestr(trace_path, "bin.oracleGeneral") != NULL ||
        strcasestr(trace_path, "oracleGeneral.zst") != NULL ||
        strcasestr(trace_path, "oracleGeneral.") != NULL ||
        strcasecmp(trace_path + strlen(trace_path) - 13, "oracleGeneral") ==
            0) {
      arg->trace_type = "oracleGeneral";
    } else if (strcasestr(trace_path, ".vscsi") != NULL) {
      arg->trace_type = "vscsi";
    } else if (strcasestr(trace_path, "oracleGeneralOpNS") != NULL) {
      arg->trace_type = "oracleGeneralOpNS";
    } else if (strcasestr(trace_path, "oracleCF1") != NULL) {
      arg->trace_type = "oracleCF1";
    } else if (strcasestr(trace_path, "oracleAkamai") != NULL) {
      arg->trace_type = "oracleAkamai";
    } else if (strcasestr(trace_path, "oracleWiki16u") != NULL) {
      arg->trace_type = "oracleWiki16u";
    } else if (strcasestr(trace_path, "oracleWiki19u") != NULL) {
      arg->trace_type = "oracleWiki19u";
    } else if (strcasestr(trace_path, ".twr.") != NULL) {
      arg->trace_type = "twr";
    } else if (strcasestr(trace_path, ".twrNS.") != NULL) {
      arg->trace_type = "twrNS";
    } else if (strcasestr(trace_path, "oracleSysTwrNS") != NULL) {
      arg->trace_type = "oracleSysTwrNS";
    } else {
      arg->trace_type = "unknown";
    }

    INFO("detecting trace type: %s\n", arg->trace_type.c_str());
  }

  static cli_arg_t parse_cli_arg(int argc, char *argv[],
                                 po::options_description &desc) {
    cli_arg_t arg;
    arg.trace_type = "UNKNOWN_TRACE";

    //  map<string, string> params;
    //
    //  regex opexp ("--([^=]*)=(.*)");
    //  cmatch opmatch;
    //  for(int i=1; i<argc; i++) {
    //    regex_match(argv[i],opmatch, opexp);
    //    if (opmatch.size()!=3) {
    //      cerr << "params need to be in form --name=value" << endl;
    //      abort();
    //    }
    //    params[opmatch[1]] = opmatch[2];
    //  }

    po::options_description general_desc("General options");
    general_desc.add_options()("help,h", "Print help messages")(
        "input_path,i", po::value<std::string>(&(arg.ipath))->required(),
        "input trace path")(
        "output_path,o",
        po::value<std::string>(&(arg.opath))->default_value("traceAnalysis"),
        "output path")(
        "trace_type,t", po::value<std::string>(&(arg.trace_type)),
        "input trace type "
        "[twr/akamai/vscsi/wiki16u/wiki19u/standardBinIII/standardBinIQI/"
        "standardBinIQIBH/"
        "oracleAkamai/oracleGeneral/oracleWiki16u/oracleWiki19u]")(
        "n_thread", po::value<int>(&(arg.n_thread))->default_value(-1),
        "the number of threads (default is -1, meaning dynamic)");

    po::options_description all_desc("all options");
    all_desc.add(desc).add(general_desc);

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, all_desc), vm);

      if (vm.count("help")) {
        std::cout << all_desc << std::endl;
        exit(0);
      }
      po::notify(vm);  // throws on error, so do after help in case

    } catch (po::error &e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cerr << all_desc << std::endl;
      exit(1);
    }

    if (arg.n_thread <= 0) {
      arg.n_thread = static_cast<int>(std::thread::hardware_concurrency());
    }

    if (arg.trace_type == "UNKNOWN_TRACE") {
      detect_trace_type(&arg);
    }

    if (arg.trace_type == "UNKNOWN_TRACE") {
      ERROR("unknown trace type, please specify it by -t\n");
      abort();
    }

    return arg;
  }

  static reader_t *create_reader(cli_arg_t *arg) {
    reader_t *reader;
    reader_init_param_t init_params;
    init_params.real_time_field = 1;
    init_params.obj_id_field = 2;
    init_params.obj_size_field = 3;

    if (arg->trace_type == "twr") {
      reader = open_trace(arg->ipath.c_str(), TWR_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "twrNS") {
      reader = open_trace(arg->ipath.c_str(), TWRNS_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "cf1") {
      reader = open_trace(arg->ipath.c_str(), CF1_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "akamai") {
      reader =
          open_trace(arg->ipath.c_str(), AKAMAI_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "wiki16u") {
      reader =
          open_trace(arg->ipath.c_str(), WIKI16u_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "wiki19u") {
      reader =
          open_trace(arg->ipath.c_str(), WIKI19u_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "wiki19t") {
      reader =
          open_trace(arg->ipath.c_str(), WIKI19t_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "vscsi") {
      reader = open_trace(arg->ipath.c_str(), VSCSI_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "standardBinIII") {
      reader = open_trace(arg->ipath.c_str(), STANDARD_III_TRACE, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "standardBinIQI") {
      reader = open_trace(arg->ipath.c_str(), STANDARD_IQI_TRACE, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "standardBinIQQ") {
      reader = open_trace(arg->ipath.c_str(), STANDARD_IQQ_TRACE, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "standardBinIQIBH") {
      reader = open_trace(arg->ipath.c_str(), STANDARD_IQIBH_TRACE, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "plainTxt") {
      reader =
          open_trace(arg->ipath.c_str(), PLAIN_TXT_TRACE, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "oracleSimTwr") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_SIM_TWR_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleSimTwrNS") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_SIM_TWRNS_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleSysTwrNS") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_SYS_TWRNS_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleCF1") {
      reader =
          open_trace(arg->ipath.c_str(), ORACLE_CF1_BIN, OBJ_ID_NUM, nullptr);
    } else if (arg->trace_type == "oracleAkamai") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_AKAMAI_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleWiki16u") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_WIKI16u_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleWiki19u") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_WIKI19u_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleWiki19t") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_WIKI19t_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleGeneral") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_GENERAL_BIN, OBJ_ID_NUM,
                          nullptr);
    } else if (arg->trace_type == "oracleGeneralOpNS") {
      reader = open_trace(arg->ipath.c_str(), ORACLE_GENERALOPNS_BIN,
                          OBJ_ID_NUM, nullptr);
    } else {
      ERROR("unknown trace type %s\n", arg->trace_type.c_str());
      abort();
    }

    return reader;
  }
};
};  // namespace cli
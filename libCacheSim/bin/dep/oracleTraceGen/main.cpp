//
// Created by jason on 3/19/21.
//


#include "../../oracleTraceGen/oracleReuseTraceGen.h"
#include "../../oracleTraceGen/oracleFreqTraceGen.h"
#include "../utils.h"

#include "../../traceReader/reader.h"

using namespace std;
using namespace oracleTraceGen;
namespace po = boost::program_options;


typedef struct {
  std::string output_trace_type;

  std::string oracle_type;
} oracleTraceGen_arg_t;

int main(int argc, char *argv[]) {
  oracleTraceGen_arg_t oarg;

  po::options_description desc("oracleTraceGen options");
  desc.add_options()
      ("output_trace_type", po::value<std::string>(&(oarg.output_trace_type))->default_value(""),
       "output_trace_type [oracleSysTwrNS/oracleAkamai/oracleWiki16u/oracleWiki19u/oracleGeneral]/oracleGeneralOpNS")
      ("oracle_type", po::value<std::string>(&(oarg.oracle_type))->default_value("reuse"), "freq/reuse");

  cli::cli_arg_t cli_arg = cli::Util::parse_cli_arg(argc, argv, desc);
  if (cli_arg.opath.empty()) {
    cli_arg.opath = cli_arg.ipath + ".oracle";
  }

  std::cout << "input traces " << cli_arg.ipath << ", type " << cli_arg.trace_type
            << ", output " << oarg.output_trace_type << std::endl;

  const char *ipath_cstr = cli_arg.ipath.c_str();
  size_t len = strlen(ipath_cstr);
  if (strcasecmp(ipath_cstr + len - 4, ".zst") == 0) {
    printf("zstd trace is not supported because of reading from the back\n"); 
    exit(1);
  }

  reader_t *reader = cli::Util::create_reader(&cli_arg);

  if (oarg.oracle_type == "freq") {
    /* this is no longer used */
    std::vector<uint64_t> time_window{TIME_WINDOW};
    OracleFreqTraceGen trace_gen(reader, cli_arg.opath, time_window, 300);
    trace_gen.run();
  } else if (oarg.oracle_type == "reuse") {
    if (oarg.output_trace_type == "oracleSysTwrNS") {
      OracleReuseTraceGen<struct reqEntryReuseRealSystemTwrNS> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, true, false);
    } else if (oarg.output_trace_type == "oracleCF1") {
      OracleReuseTraceGen<struct reqEntryReuseCF1> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, true);
    } else if (oarg.output_trace_type == "oracleAkamai") {
      OracleReuseTraceGen<struct reqEntryReuseAkamai> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, true);
    } else if (oarg.output_trace_type == "oracleWiki16u") {
      OracleReuseTraceGen<struct reqEntryReuseWiki16u> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, true);
    } else if (oarg.output_trace_type == "oracleWiki19u") {
      OracleReuseTraceGen<struct reqEntryReuseWiki19u> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, true);
    } else if (oarg.output_trace_type == "oracleGeneral") {
      OracleReuseTraceGen<struct reqEntryReuseGeneral> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, true);
    } else if (oarg.output_trace_type == "oracleGeneralOpNS") {
      OracleReuseTraceGen<struct reqEntryReuseGeneralOpNS> trace_gen(reader, cli_arg.opath);
      trace_gen.run(false, false, false);
    } else {
      std::cerr << "unknown output type " << oarg.output_trace_type << std::endl;
      abort();
    }
  } else {
    std::cerr << "unknown oracle " << oarg.oracle_type << std::endl;
    abort();
  }
}




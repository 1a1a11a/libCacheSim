

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.hpp"
#include "libCacheSim/reader.h"

/**
 * @brief convert a given trace to lcs format
 *
 * there are multiple versions of lcs format see lcs.h for more details
 * each version has a different request struct format, however, all lcs traces
 * have the same header format which stores the version and trace statistics
 *
 *
 * see traceReader/generalReader/lcs.h for more details
 *
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char *argv[]) {
  struct arguments args;

  cli::parse_cmd(argc, argv, &args);
  if (strlen(args.ofilepath) == 0) {
    snprintf(args.ofilepath, OFILEPATH_LEN, "%s.%s", args.trace_path,
             args.output_format);
  }

  INFO("output format %s, output path %s\n", args.output_format,
       args.ofilepath);
  if (strcasecmp(args.output_format, "lcs") == 0 ||
      strcasecmp(args.output_format, "lcs_v1") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 1);
  } else if (strcasecmp(args.output_format, "lcs_v2") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 2);
  } else if (strcasecmp(args.output_format, "lcs_v3") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 3);
  } else if (strcasecmp(args.output_format, "lcs_v4") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 4);
  } else if (strcasecmp(args.output_format, "lcs_v5") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 5);
  } else if (strcasecmp(args.output_format, "lcs_v6") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 6);
  } else if (strcasecmp(args.output_format, "lcs_v7") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 7);
  } else if (strcasecmp(args.output_format, "lcs_v8") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt,
                              args.remove_size_change, 8);
  } else if (strcasecmp(args.output_format, "oracleGeneral") == 0) {
    traceConv::convert_to_oracleGeneral(
        args.reader, args.ofilepath, args.output_txt, args.remove_size_change);
  } else {
    ERROR("unknown output format %s\n", args.output_format);
    exit(1);
  }
}

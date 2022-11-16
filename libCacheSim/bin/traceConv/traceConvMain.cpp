

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "internal.hpp"


int main(int argc, char *argv[]) {
  struct arguments args;

  cli::parse_cmd(argc, argv, &args);
  if (args.ofilepath == NULL) {
    args.ofilepath = (char *)malloc(strlen(args.trace_path) + 10);
    sprintf(args.ofilepath, "%s.lcs", args.trace_path);
  }

  traceConv::convert_to_oracleGeneral(args.reader, args.ofilepath,
                                      args.sample_ratio, args.output_txt,
                                      args.remove_size_change, false);
}




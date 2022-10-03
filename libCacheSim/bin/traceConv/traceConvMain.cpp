

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "internal.hpp"

// # convert to oracleGeneral
// # downsampling
// # zstd trace

int main(int argc, char *argv[]) {
  struct arguments args;

  cli::parse_cmd(argc, argv, &args);

  oracleGeneral::convert_to_oracleGeneral(args.reader, args.ofilepath,
                                          args.sample_ratio, args.output_txt,
                                          args.remove_size_change);
}

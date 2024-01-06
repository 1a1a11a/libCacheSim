

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "internal.hpp"

/**
 * @brief convert a given trace to oracleGeneral format
 * 
 * oracleGeneral is a binary format with each request stored using the following struct
 * struct oracleGeneral {
 *   uint32_t wallclock_time;
 *   uint64_t obj_id;
 *   uint32_t obj_size;
 *   int64_t next_access_logical_timestamp;
 * }; 
 * 
 * see traceReader/customizedReader/oracle/oracleGeneral.h for more details
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
    snprintf(args.ofilepath, OFILEPATH_LEN, "%s.oracleGeneral", args.trace_path);
  }

  traceConv::convert_to_oracleGeneral(args.reader, args.ofilepath,
                                      args.sample_ratio, args.output_txt,
                                      args.remove_size_change, false);
}




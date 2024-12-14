

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "internal.hpp"

/**
 * @brief convert a given trace to lcs format
 *
 * there are multiple versions of lcs format see lcs.h for more details
 * each version has a different request struct format, however, all lcs traces have
 * the same header format which stores the version and trace statistics
 *
 * lcs_v1 is the simplest format with only clock_time, obj_id, obj_size, and next_access_vtime
 *
 * typedef struct __attribute__((packed)) lcs_req_v1 {
 *   uint32_t clock_time;
 *   uint64_t obj_id;
 *   uint32_t obj_size;
 *   int64_t next_access_vtime;
 * } lcs_req_v1_t;
 *
 *
 * lcs_v2 has more fields, operation and tenant
 *
 * typedef struct __attribute__((packed)) lcs_req_v2 {
 *   uint32_t clock_time;
 *   uint64_t obj_id;
 *   uint32_t obj_size;
 *   uint32_t op : 8;
 *   uint32_t tenant : 24;
 *   int64_t next_access_vtime;
 * } lcs_req_v2_t;
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
    snprintf(args.ofilepath, OFILEPATH_LEN, "%s.%s", args.trace_path, args.output_format);
  }

  if (strcasecmp(args.output_format, "lcs") == 0 || strcasecmp(args.output_format, "lcs_v1") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt, args.remove_size_change, 1);
  } else if (strcasecmp(args.output_format, "lcs_v2") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt, args.remove_size_change, 2);
  } else if (strcasecmp(args.output_format, "lcs_v3") == 0) {
    traceConv::convert_to_lcs(args.reader, args.ofilepath, args.output_txt, args.remove_size_change, 3);
  } else if (strcasecmp(args.output_format, "oracleGeneral") == 0) {
    traceConv::convert_to_oracleGeneral(args.reader, args.ofilepath, args.output_txt, args.remove_size_change);
  } else {
    ERROR("unknown output format %s\n", args.output_format);
    exit(1);
  }
}

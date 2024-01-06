/**
 * Filter the trace to generate a second-layer cache trace.
 * only support single algorithm (without params), e.g., FIFO and LRU, at this
 * moment. The output format is lcs format (oracleGeneral).
 *
 */

#include <libgen.h>

#include <fstream>
#include <iostream>
#include <string>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../cli_reader_utils.h"
#include "internal.hpp"

namespace TraceFilter {
struct output_format {
  uint32_t clock_time;
  uint64_t obj_id;
  uint32_t obj_size;
  int64_t next_access_vtime;
} __attribute__((packed));

void filter(reader_t *reader, cache_t *cache, std::string ofilepath) {
  request_t *req = new_request();

  std::ofstream output_file(ofilepath, std::ios::binary);
  struct output_format output_req;
  output_req.next_access_vtime = -2;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;

  int64_t n_req = 0, n_written_req = 0;
  while (req->valid) {
    n_req++;
    req->clock_time -= start_ts;
    if (cache->get(cache, req) == false) {
      output_req.clock_time = req->clock_time;
      output_req.obj_id = req->obj_id;
      output_req.obj_size = req->obj_size;
      output_file.write((char *)&output_req, sizeof(struct output_format));
      n_written_req++;
    }

    read_one_req(reader, req);
  }

  INFO("write %ld/%ld %.4lf requests to file %s\n", (long) n_written_req, (long) n_req,
       (double)n_written_req / n_req, ofilepath.c_str());
  free_request(req);
  output_file.close();
}
}  // namespace TraceFilter

int main(int argc, char *argv[]) {
  struct arguments args;
  cli::parse_cmd(argc, argv, &args);

  if (args.cache_size < 1) {
    int64_t wss_obj = 0, wss_byte = 0;
    cal_working_set_size(args.reader, &wss_obj, &wss_byte);
    if (args.ignore_obj_size) {
      args.cache_size = (int64_t)(wss_obj * args.cache_size);
    } else {
      args.cache_size = (int64_t)(wss_byte * args.cache_size);
    }
  }

  common_cache_params_t cc_params = {
      .cache_size = static_cast<uint64_t>(args.cache_size),
      .default_ttl = 86400 * 300,
      .hashpower = 24,
      .consider_obj_metadata = false};

  if (strcasecmp(args.cache_name, "LRU") == 0) {
    args.cache = LRU_init(cc_params, NULL);
  } else if (strcasecmp(args.cache_name, "FIFO") == 0) {
    args.cache = FIFO_init(cc_params, NULL);
  } else {
    ERROR("unsupported cache name %s\n", args.cache_name);
    exit(1);
  }

  if (args.ofilepath[0] == '\0') {
    char *trace_filename = rindex(args.trace_path, '/');
    snprintf(args.ofilepath, OFILEPATH_LEN, "%s.filter_%s_%lu.oracleGeneral",
             trace_filename == NULL ? args.trace_path : trace_filename + 1,
             args.cache_name, static_cast<unsigned long>(args.cache_size));
  }

  TraceFilter::filter(args.reader, args.cache, args.ofilepath);

  cli::free_arg(&args);

  return 0;
}

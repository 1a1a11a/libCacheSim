#pragma once

#include <inttypes.h>

#include <string>

#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/cache.h"

#define N_ARGS 2
#define OFILEPATH_LEN 128

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  int64_t n_req;
  char *args[N_ARGS];
  char *trace_path;
  char ofilepath[OFILEPATH_LEN];
  char *trace_type_str;
  trace_type_e trace_type;
  char *trace_type_params;
  double sample_ratio;
  bool ignore_obj_size;

  /* trace conv */
  bool output_txt;
  /* some objects may change size during the trace, this keeps the size as the
   * last size in the trace */
  bool remove_size_change;

  /* trace print */
  int64_t num_req; /* number of requests to print */
  char delimiter;
  bool print_obj_id_only;
  bool print_obj_id_32bit;

  /* trace filter */
  char *cache_name;
  double cache_size;
  cache_t *cache;

  /* arguments generated */
  reader_t *reader;
};

namespace cli {
void parse_cmd(int argc, char *argv[], struct arguments *args);

void free_arg(struct arguments *args);
}  // namespace cli


namespace traceConv {

/**
 * @brief convert the trace to oracleGeneral format
 *
 *
 * @param reader
 * @param ofilepath
 * @param sample_ratio
 * @param output_txt    whether also output a txt trace
 * @param remove_size_change whether remove object size change during traceConv
 * @param use_lcs_format whether use lcs format
 */
void convert_to_oracleGeneral(reader_t *reader, std::string ofilepath,
                              int sample_ratio, bool output_txt,
                              bool remove_size_change, bool use_lcs_format);

}  // namespace traceConv

#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"

#define N_ARGS 3
#define OFILEPATH_LEN 128
#define TASK_STR_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  char ofilepath[OFILEPATH_LEN];
  trace_type_e trace_type;
  char *trace_type_params;
    char task[TASK_STR_LEN];
  int64_t n_req; /* number of requests to process */
  bool verbose;

  /* arguments generated */
  reader_t *reader;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

void cal_one_hit(reader_t *reader, char *ofilepath);


#ifdef __cplusplus
}
#endif

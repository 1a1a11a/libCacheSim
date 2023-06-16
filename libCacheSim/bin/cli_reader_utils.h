#pragma once 

#include "../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

trace_type_e trace_type_str_to_enum(const char *trace_type_str, const char *trace_path);

bool is_true(const char *arg);

void parse_reader_params(char *reader_params_str, reader_init_param_t *params);

trace_type_e detect_trace_type(const char *trace_path);

bool should_disable_obj_metadata(reader_t *reader); 

#ifdef __cplusplus
}
#endif


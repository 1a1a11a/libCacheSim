#pragma once 

#include "../include/libCacheSim/reader.h"

trace_type_e trace_type_str_to_enum(const char *trace_type_str);

bool is_true(const char *arg);

void parse_reader_params(char *reader_params_str, reader_init_param_t *params);

void verify_trace_type(const char *trace_path, const char *trace_type_str);

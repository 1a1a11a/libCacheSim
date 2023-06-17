#pragma once

#include "../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

trace_type_e trace_type_str_to_enum(const char *trace_type_str,
                                    const char *trace_path);

bool is_true(const char *arg);

void parse_reader_params(const char *reader_params_str,
                         reader_init_param_t *params);

trace_type_e detect_trace_type(const char *trace_path);

bool should_disable_obj_metadata(reader_t *reader);

void cal_working_set_size(reader_t *reader, int64_t *wss_obj,
                          int64_t *wss_byte);

reader_t *create_reader(const char *trace_type_str, const char *trace_path,
                        const char *trace_type_params, const int64_t n_req,
                        const bool ignore_obj_size, const int sample_ratio);

#ifdef __cplusplus
}
#endif

//
//  reader.h
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef READER_H
#define READER_H

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "const.h"
#include "enum.h"
#include "logging.h"
#include "request.h"
#include "sampling.h"

#ifdef __cplusplus
extern "C" {
#endif

/* this provides the info about each field or col in csv and binary trace
 * the field index start with 1 */
typedef struct {
  bool ignore_obj_size;
  bool ignore_size_zero_req;
  bool obj_id_is_num;
  bool obj_id_is_num_set;  // whether the user has passed this parameter
  int64_t cap_at_n_req;    // only process at most n_req requests

  int32_t time_field;
  int32_t obj_id_field;
  int32_t obj_size_field;
  int32_t op_field;
  int32_t ttl_field;
  int32_t cnt_field;
  int32_t tenant_field;
  int32_t next_access_vtime_field;

  int32_t n_feature_fields;
  int32_t feature_fields[N_MAX_FEATURES];

  // block cache, 0 and -1 means ignore this field, 1 is also invalid
  // block_size breaks a large request for multiple blocks into multiple requests
  int32_t block_size;

  // csv reader
  bool has_header;
  // whether the has_header is set, because false could indicate
  // it is not set or it does not has a header
  bool has_header_set;

  char delimiter;
  // read the trace from the offset, this is used by some binary trace
  // which stores metadata at the start of the trace
  ssize_t trace_start_offset;

  // binary reader
  char *binary_fmt_str;

  // sample some requests in the trace
  sampler_t *sampler;
} reader_init_param_t;

enum read_direction {
  READ_FORWARD = 0,
  READ_BACKWARD = 1,
};

struct zstd_reader;
typedef struct reader {
  /************* common fields *************/
  uint64_t n_read_req;
  uint64_t n_total_req; /* number of requests in the trace */
  char *trace_path;
  size_t file_size;
  reader_init_param_t init_params;
  void *reader_params;
  trace_type_e trace_type; /* possible types see trace_type_t  */
  trace_format_e trace_format;
  int ver;
  bool cloned;  // true if this is a cloned reader, else false
  int64_t cap_at_n_req;
  /* the offset of the first request in the trace, it should be 0 for
   *    txt trace
   *    csv trace with no header
   *    customized binary traces
   * but may not be 0 for
   *    csv trace with header
   *    LCS trace
   * this is used when cloning reader and reading reversely */
  int trace_start_offset;

  /************* used by binary trace *************/
  /* mmap the file, this should not change during runtime */
  char *mapped_file;
  size_t mmap_offset;
  struct zstd_reader *zstd_reader_p;
  bool is_zstd_file;
  /* the size of one request in binary trace */
  size_t item_size;

  /************* used by txt trace *************/
  FILE *file;
  char *line_buf;
  size_t line_buf_size;
  char csv_delimiter;
  bool csv_has_header;

  /* whether the object id is numeric value */
  bool obj_id_is_num;
  /* whether obj_id_is_num is set by user */
  bool obj_id_is_num_set;

  bool ignore_size_zero_req;
  /* if true, ignore the obj_size in the trace, and use size one */
  bool ignore_obj_size;

  // used by block cache trace to split a large request into multiple requests to multiple blocks
  int32_t block_size;

  /* this is used when
   * a) the reader splits a large req into multiple chunked requests
   * b) the trace file uses a count field */
  int n_req_left;
  int64_t last_req_clock_time;

  // lcs trace version, used only lcs reader
  int64_t lcs_ver;

  /* used for trace sampling */
  sampler_t *sampler;
  enum read_direction read_direction;
} reader_t;

static inline void set_default_reader_init_params(reader_init_param_t *params) {
  memset(params, 0, sizeof(reader_init_param_t));

  params->ignore_obj_size = false;
  params->ignore_size_zero_req = true;
  params->obj_id_is_num = true;
  params->obj_id_is_num_set = false;
  params->cap_at_n_req = -1;
  params->trace_start_offset = 0;

  params->has_header = false;
  /* whether the user has specified the has_header params */
  params->has_header_set = false;
  params->delimiter = ',';

  params->block_size = -1;
  params->binary_fmt_str = NULL;

  params->sampler = NULL;
}

static inline reader_init_param_t default_reader_init_params(void) {
  reader_init_param_t init_params;
  set_default_reader_init_params(&init_params);

  return init_params;
}

/**
 * setup a reader for reading trace
 * @param trace_path path to the trace
 * @param trace_type CSV_TRACE, PLAIN_TXT_TRACE, BIN_TRACE, VSCSI_TRACE,
 *  TWR_BIN_TRACE, see libCacheSim/enum.h for more
 * @param obj_id_type OBJ_ID_NUM, OBJ_ID_STR,
 *  used by CSV_TRACE and PLAIN_TXT_TRACE, whether the obj_id in the trace is a
 *  number or not, if it is not a number then we will map it to uint64_t
 * @param reader_init_param some initialization parameters used by csv and
 * binary traces these include time_field, obj_id_field, obj_size_field,
 * op_field, ttl_field, has_header, delimiter, binary_fmt_str
 *
 * @return a pointer to reader_t struct, the returned reader needs to be
 * explicitly closed by calling close_reader or close_trace
 */
reader_t *setup_reader(const char *trace_path, trace_type_e trace_type, const reader_init_param_t *reader_init_param);

/* this is the same function as setup_reader */
static inline reader_t *open_trace(const char *path, const trace_type_e type,
                                   const reader_init_param_t *reader_init_param) {
  return setup_reader(path, type, reader_init_param);
}

/**
 * get the number of requests from the trace
 * @param reader
 * @return
 */
uint64_t get_num_of_req(reader_t *reader);

/**
 * get the trace type
 * @param reader
 * @return
 */
static inline trace_type_e get_trace_type(const reader_t *const reader) { return reader->trace_type; }

/**
 * whether the object id is numeric (only applies to txt and csv traces)
 * @param reader
 * @return
 */
static inline bool obj_id_is_num(const reader_t *const reader) { return reader->obj_id_is_num; }

/**
 * read one request from reader/trace, stored the info in pre-allocated req
 * @param reader
 * @param req
 * return 0 on success and 1 if reach end of trace
 */
int read_one_req(reader_t *reader, request_t *req);

/**
 * read one request from reader/trace, stored the info in pre-allocated req
 * @param reader
 * @param req
 * return 0 on success and 1 if reach end of trace
 */
static inline int read_trace(reader_t *const reader, request_t *const req) { return read_one_req(reader, req); }

/**
 * reset reader, so we can read from the beginning
 * @param reader
 */
void reset_reader(reader_t *reader);

/**
 * close reader and release resources
 * @param reader
 * @return
 */
int close_reader(reader_t *reader);

static inline int close_trace(reader_t *const reader) { return close_reader(reader); }

/**
 * clone a reader, mostly used in multithreading
 * @param reader
 * @return
 */
reader_t *clone_reader(const reader_t *reader);

void read_first_req(reader_t *reader, request_t *req);

void read_last_req(reader_t *reader, request_t *req);

int skip_n_req(reader_t *reader, int N);

int read_one_req_above(reader_t *reader, request_t *c);

int go_back_one_req(reader_t *reader);

void reader_set_read_pos(reader_t *reader, double pos);

static inline void print_reader(reader_t *reader) {
  printf(
      "trace_type: %s, trace_path: %s, trace_start_offset: %d, mmap_offset: "
      "%lu, is_zstd_file: %d, item_size: %zu, file: %p, line_buf: "
      "%p, line_buf_size: %zu, csv_delimiter: %c, csv_has_header: %d, "
      "obj_id_is_num: %d, ignore_size_zero_req: %d, ignore_obj_size: %d, "
      "n_req_left: %d, last_req_clock_time: %ld\n",
      g_trace_type_name[reader->trace_type], reader->trace_path, reader->trace_start_offset, (long)reader->mmap_offset,
      reader->is_zstd_file, reader->item_size, reader->file, reader->line_buf, reader->line_buf_size,
      reader->csv_delimiter, reader->csv_has_header, reader->obj_id_is_num, reader->ignore_size_zero_req,
      reader->ignore_obj_size, reader->n_req_left, (long)reader->last_req_clock_time);
}

#ifdef __cplusplus
}
#endif

#endif /* reader_h */

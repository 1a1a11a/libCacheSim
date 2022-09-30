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

  int time_field;
  int obj_id_field;
  int obj_size_field;
  int op_field;
  int ttl_field;

  int extra_field1;
  int extra_field2;

  // csv reader
  bool has_header;
  // whether the has_header is set because false could indicate
  // it is not set or it does not has a header
  bool has_header_set;
  char delimiter;

  // binary reader
  char *binary_fmt;
} reader_init_param_t;

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

  /************* used by binary trace *************/
  char *mapped_file; /* mmap the file, this should not change during runtime */
  uint64_t mmap_offset;
  struct zstd_reader *zstd_reader_p;
  bool is_zstd_file;
  size_t item_size; /* the size of one request in binary trace, used to
                     * locate the memory location of next element,
                     * when used in vscsiReaser and binaryReader,
                     * it is a const value,
                     * when it is used in plainReader or csvReader,
                     * it is the size of last record, it does not
                     * include LFCR or \0 */

  /************* used by txt trace *************/
  FILE *file;
  char *line_buf;
  size_t line_buf_size;
  char csv_delimiter;
  bool csv_has_header;
  /* whether the object id is hashed */
  bool obj_id_is_num;
  obj_id_type_e obj_id_type; /* possible types see obj_id_type_e in request.h */

  bool ignore_size_zero_req;
  /* if true, ignore the obj_size in the trace, and use size one */
  bool ignore_obj_size;

  /* this is used when the reader splits a large req into multiple chunked
   * requests */
  int n_chunked_req_left;
  int64_t chunked_req_clock_time;


  trace_sampling_func sampling_func; /* used for sampling */

} reader_t;

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
 * op_field, ttl_field, has_header, delimiter, binary_fmt
 *
 * @return a pointer to reader_t struct, the returned reader needs to be
 * explicitly closed by calling close_reader or close_trace
 */
reader_t *setup_reader(const char *trace_path, trace_type_e trace_type,
                       obj_id_type_e obj_id_type,
                       const reader_init_param_t *reader_init_param);

/* this is the same function as setup_reader */
static inline reader_t *open_trace(const char *path, const trace_type_e type,
                                   const obj_id_type_e obj_id_type,
                                   reader_init_param_t *reader_init_param) {
  return setup_reader(path, type, obj_id_type, reader_init_param);
}

/**
 * add a sampling_func to the reader, the requests from the reader will be
 * sampled
 * @param reader
 * @param sampling_func
 */
static inline void add_sampling(reader_t *reader,
                                trace_sampling_func sampling_func) {
  reader->sampling_func = sampling_func;
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
static inline trace_type_e get_trace_type(const reader_t *const reader) {
  return reader->trace_type;
}

/**
 * get the obj_id type, it can be OBJ_ID_NUM or OBJ_ID_STR
 * @param reader
 * @return
 */
static inline obj_id_type_e get_obj_id_type(const reader_t *const reader) {
  return reader->obj_id_type;
}

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
static inline int read_trace(reader_t *const reader, request_t *const req) {
  return read_one_req(reader, req);
}

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

static inline int close_trace(reader_t *const reader) {
  return close_reader(reader);
}

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

// Jason: need to get rid of this, this is to change csv reader
void set_no_eof(reader_t *reader);

int go_back_one_line(reader_t *reader);

void reader_set_read_pos(reader_t *reader, double pos);

// static inline uint64_t str_to_obj_id(char *s, size_t len) {
//   return (uint64_t) atoll(s);
//   ERROR("not supported yet\n");
//   abort();
//   return 0;
// }

#ifdef __cplusplus
}
#endif

#endif /* reader_h */

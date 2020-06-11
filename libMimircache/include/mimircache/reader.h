//
//  reader.h
//  libMimircache
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef READER_H
#define READER_H


#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <glib.h>
#include <gmodule.h>

#include "const.h"
#include "enum.h"
#include "request.h"
#include "logging.h"


#define FILE_TAB 0x09
#define FILE_SPACE 0x20
#define FILE_CR 0x0d
#define FILE_LF 0x0a
#define FILE_COMMA 0x2c
#define FILE_QUOTE 0x22




typedef struct {
  int real_time_field;
  int obj_id_field;
  int obj_size_field;
  int op_field;
  int ttl_field;

  // csv reader
  gboolean has_header;
  char delimiter;
  // binary reader
  char binary_fmt[MAX_BIN_FMT_STR_LEN];
} reader_init_param_t;


/* declare reader struct */
struct reader;
typedef struct reader_base {

  char trace_path[MAX_FILE_PATH_LEN];
  reader_init_param_t init_params;

  trace_type_t trace_type; /* possible types see trace_type_t  */
  obj_id_type_t obj_id_type; /* possible types see obj_id_type_t in request.h */

  FILE *file;
  size_t file_size;
  char *mapped_file; /* mmap the file, this should not change during runtime
                      * mmap_offset in the file is changed using mmap_offset */
  guint64 mmap_offset;

  size_t item_size; /* the size of one record, used to
                       * locate the memory location of next element,
                       * when used in vscsiReaser and binaryReader,
                       * it is a const value,
                       * when it is used in plainReader or csvReader,
                       * it is the size of last record, it does not
                       * include LFCR or 0 */

  gint64 n_total_req; /* number of requests in the trace */


  gint ver;
  void *other_params; /* currently not used */

} reader_base_t;


typedef struct reader {
  struct reader_base *base;
  void *reader_params;
  gboolean cloned;    // True if this is a cloned reader, else false
} reader_t;


/**
 * setup the reader struct for reading trace
 * @param trace_path
 * @param trace_type CSV_TRACE, PLAIN_TXT_TRACE, BIN_TRACE, VSCSI_TRACE, TWR_TRACE
 * @param obj_id_type OBJ_ID_NUM, OBJ_ID_STR
 * @param setup_params
 * @return a pointer to READER struct, the returned reader needs to be explicitly closed by calling close_reader
 */
reader_t *setup_reader(const char *trace_path,
                       const trace_type_t trace_type,
                       const obj_id_type_t obj_id_type,
                       const reader_init_param_t *const reader_init_param);


/**
 * read one request from reader, and store it in the pre-allocated request_t req
 * @param reader
 * @param req
 */
void read_one_req(reader_t *const reader, request_t *const req);

guint64 skip_N_elements(reader_t *const reader, const guint64 N);

int go_back_one_line(reader_t *const reader);

int go_back_two_lines(reader_t *const reader);

void read_one_req_above(reader_t *const reader, request_t *c);

// deprecated
void reader_set_read_pos(reader_t *const reader, double pos);

guint64 get_num_of_req(reader_t *const reader);

void reset_reader(reader_t *const reader);

int close_reader(reader_t *const reader);

reader_t *clone_reader(const reader_t *const reader);

// Jason: what is this for? using it for Optimal alg is not ideal
void set_no_eof(reader_t *const reader);

gboolean find_line_ending(reader_t *const reader, char **line_end,
                          size_t *const line_len);

static inline uint64_t str_to_u64(const char *start, size_t len) {
  uint64_t n = 0;
  while (len--) {
    n = n * 10 + *start++ - '0';
  }
  return n;
}



#ifdef __cplusplus
}
#endif

#endif /* reader_h */

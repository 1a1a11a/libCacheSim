//
//  reader.h
//  LRUAnalyzer
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
#include <glib.h>
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
#include "utils.h"
#include "request.h"
#include "logging.h"


#define FILE_TAB 0x09
#define FILE_SPACE 0x20
#define FILE_CR 0x0d
#define FILE_LF 0x0a
#define FILE_COMMA 0x2c
#define FILE_QUOTE 0x22


// trace type
typedef enum {
  CSV_TRACE = 'c', BIN_TRACE = 'b', VSCSI_TRACE = 'v', PLAIN_TXT_TRACE = 'p'
} trace_type_t;


/* declare reader struct */
struct reader;
typedef struct reader_base {

  trace_type_t trace_type; /* possible types see trace_type_t  */
  obj_id_t obj_id_type; /* possible types see obj_id_t in request.h */

//  int block_unit_size; /* used when consider variable request size
//                        * it is size of basic unit of a big request,
//                        * in CPHY data, it is 512 bytes */
  /* currently not used */
//  int disk_sector_size;

  FILE *file;
  char trace_path[MAX_FILE_PATH_LEN];
  //    char rd_file_loc[MAX_FILE_PATH_LEN];    /* the file that stores reuse
  //    distance of the data,
  //                                             * format: gint64 per entry */
  //    char frd_file_loc[MAX_FILE_PATH_LEN];
  size_t file_size;
  void *init_params;

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

typedef struct reader_data_unique {

//  double *hit_ratio;
//  double *hit_ratio_shards;
//  double log_base;

} reader_data_unique_t;

typedef struct reader_data_share {
  break_point_t *break_points;
//  gint64 *reuse_dist;
//  dist_t reuse_dist_type;
//  gint64 max_reuse_dist;
//  gint64 *last_access;

} reader_data_share_t;

typedef struct reader {
  struct reader_base *base;
//  struct reader_data_unique *udata;
  struct reader_data_share *sdata;
  void *reader_params;
} reader_t;


typedef struct {
  gint obj_id_pos;                     // ordering begins with 0
  gint op_pos;
  gint real_time_pos;
  gint size_pos;
  gint unused_pos1;
  gint unused_pos2;
  char fmt[MAX_BIN_FMT_STR_LEN];
} binary_init_params_t;

typedef struct{
  gint obj_id_field;
  gint op_field;
  gint real_time_field;
  gint size_field;
  gboolean has_header;
  unsigned char delimiter;
  gint traceID_field;
}csvReader_init_params;










reader_t *setup_reader(const char *file_loc, const trace_type_t trace_type,
                       const obj_id_t obj_id_type,
                       const void *const setup_params);

void read_one_req(reader_t *const reader, request_t *const req);

guint64 skip_N_elements(reader_t *const reader, const guint64 N);

int go_back_one_line(reader_t *const reader);

int go_back_two_lines(reader_t *const reader);

void read_one_req_above(reader_t *const reader, request_t *c);

// deprecated
int read_one_request_all_info(reader_t *const reader, void *storage);

guint64 read_one_timestamp(reader_t *const reader);

void read_one_op(reader_t *const reader, void *op);

guint64 read_one_request_size(reader_t *const reader);

// deprecated
void reader_set_read_pos(reader_t *const reader, double pos);

guint64 get_num_of_req(reader_t *const reader);

void reset_reader(reader_t *const reader);

int close_reader(reader_t *const reader);

int close_cloned_reader(reader_t *const reader);

reader_t *clone_reader(reader_t *const reader);

// Jason: what is this for? using it for Optimal alg is not good
void set_no_eof(reader_t *const reader);

gboolean find_line_ending(reader_t *const reader, char **line_end,
                          size_t *const line_len);

static inline gsize str_to_gsize(const char *start, size_t len) {
  gsize n = 0;
  while (len--){
#ifdef DEBUG_MODE
    if (*start < '0' || *start > '9'){
      ERROR("cannot convert string to gsize, current char %c\n", *start);
      abort();
    }
#endif
    n = n * 10 + *start++ - '0';
//    printf("ptr %p - len %lu - char %c - value %lu\n", start, len, *(start-1), n);
  }
  return n;
}


#ifdef __cplusplus
}
#endif

#endif /* reader_h */

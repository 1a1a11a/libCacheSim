#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include "../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PER_SEEK_SIZE 256
#define MAX_OBJ_ID_LEN 256

/**************** common ****************/
bool is_str_num(const char *str, size_t len);

/**************** csv ****************/
typedef struct {
  struct csv_parser *csv_parser;

  /* used in cb1, each time a field is read, curr_field_idx incr */
  int curr_field_idx;

  int time_field_idx;
  int obj_id_field_idx;
  int obj_size_field_idx;
  int op_field_idx;
  int cnt_field_idx;
  int ttl_field_idx;
  int tenant_field_idx;

  int n_feature_fields;
  int feature_fields[N_MAX_FEATURES];

  bool has_header;
  unsigned char delimiter;

  int n_obj_id_is_num;
  int n_obj_id_is_not_num;

  void *request;
} csv_params_t;

void csv_setup_reader(reader_t *const reader);

int csv_read_one_req(reader_t *const, request_t *const);

void csv_reset_reader(reader_t *reader);

/**
 * check whether the trace uses the given delimiter
 * @param reader
 * @param delimiter
 * @return
 */
bool check_delimiter(const reader_t *reader, char delimiter);

/**************** txt ****************/
int txt_read_one_req(reader_t *const reader, request_t *const req);

/**************** binary ****************/
static inline int format_to_size(char format) {
  switch (format) {
    case 'c':
    case 'b':
    case 'B':
      return 1;
    case 'h':
    case 'H':
      return 2;
    case 'i':
    case 'I':
    case 'l':
    case 'L':
    case 'f':
      return 4;
    case 'q':
    case 'Q':
    case 'd':
      return 8;
    default:
      ERROR("unknown format '%c'\n", format);
  }
}

typedef struct {
  int32_t time_offset;
  int8_t time_field_idx;
  char time_format;

  int32_t obj_id_offset;
  int8_t obj_id_field_idx;
  char obj_id_format;

  int32_t op_offset;
  int8_t op_field_idx;
  char op_format;

  int32_t ttl_offset;
  int8_t ttl_field_idx;
  char ttl_format;

  int32_t next_access_vtime_offset;
  int8_t next_access_vtime_field_idx;
  char next_access_vtime_format;

  int32_t obj_size_offset;
  int8_t obj_size_field_idx;
  char obj_size_format;

  int32_t n_fields;
  int32_t item_size;
  char *fmt_str;
} binary_params_t;

/* function to setup binary reader */
int binaryReader_setup(reader_t *const reader);

int binary_read_one_req(reader_t *reader, request_t *req);

#ifdef __cplusplus
}
#endif

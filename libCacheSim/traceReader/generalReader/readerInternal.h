#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINE_LEN (1024 * 1)
#define MAX_OBJ_ID_LEN 256


/**************** common ****************/
bool is_str_num(const char *str);

/**************** csv ****************/
typedef struct {
  struct csv_parser *csv_parser;

  /* used in cb1, each time a field is read, curr_field_idx incr */
  int curr_field_idx;

  int time_field_idx;
  int obj_id_field_idx;
  int obj_size_field_idx;
  bool has_header;
  unsigned char delimiter;

  void *request;
} csv_params_t;

bool csv_detect_obj_id_is_num(reader_t *const reader);

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
typedef struct {
  int32_t obj_id_field;  // the beginning bytes in the struct
  uint32_t obj_id_len;   // the size of obj_id
  char obj_id_type;

  int32_t op_field;
  uint32_t op_len;
  char op_type;

  int32_t time_field;
  uint32_t time_len;
  char time_type;

  int32_t obj_size_field;
  uint32_t obj_size_len;
  char obj_size_type;

  int32_t ttl_field;
  uint32_t ttl_len;
  char ttl_type;

  char *fmt;
  uint32_t num_of_fields;
} binary_params_t;

/* function to setup binary reader */
int binaryReader_setup(reader_t *const reader);

int binary_read_one_req(reader_t *reader, request_t *req);

#ifdef __cplusplus
}
#endif

//
//  csvReader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "csv.h"
#include "libcsv.h"
#include "../readerInternal.h"

#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void csv_cb1(void *s, size_t len, void *data) {
  /* call back for csv field end */

  reader_t *reader = (reader_t *) data;
  csv_params_t *params = reader->reader_params;
  reader_init_param_t *init_params = &reader->init_params;
  request_t *req = params->req_pointer;

  if (params->current_field_counter == init_params->obj_id_field) {
    if (reader->obj_id_type == OBJ_ID_NUM) {
      // len is not correct for the last request for some reason
      req->obj_id = str_to_obj_id((char *) s, len);
    } else {
      if (len >= MAX_OBJ_ID_LEN) {
        len = MAX_OBJ_ID_LEN - 1;
        ERROR("csvReader obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n", len,
              MAX_OBJ_ID_LEN);
        abort();
      }
      char obj_id_str[MAX_OBJ_ID_LEN];
      memcpy(obj_id_str, (char *) s, len);
      obj_id_str[len] = 0;
      req->obj_id = (uint64_t) g_quark_from_string(obj_id_str);
    }
    params->already_got_req = true;
  } else if (params->current_field_counter == init_params->real_time_field) {
    // this does not work, because s is not null terminated
    uint64_t ts = (uint64_t) atof((char *) s);
    // we only support 32-bit ts
    assert(ts < UINT32_MAX);
    req->real_time = ts;
  } else if (params->current_field_counter == init_params->op_field) {
    fprintf(stderr, "currently operation column is not supported\n");
  } else if (params->current_field_counter == init_params->obj_size_field) {
    req->obj_size = (guint64) atoll((char *) s);
  }

  params->current_field_counter++;
}

static inline void csv_cb2(int c, void *data) {
  /* call back for csv row end */

  reader_t *reader = (reader_t *) data;
  csv_params_t *params = reader->reader_params;
  params->current_field_counter = 1;

  /* move the following code to csv_cb1 after detecting obj_id,
   * because putting here will cause a bug when there is no new line at the
   * end of file, then the last line will have an incorrect reference number
   */
}

void csv_setup_reader(reader_t *const reader) {
  reader->trace_format = TXT_TRACE_FORMAT;
  unsigned char options = CSV_APPEND_NULL;
  reader->reader_params = g_new0(csv_params_t, 1);
  csv_params_t *params = reader->reader_params;
  reader_init_param_t *init_params = &reader->init_params;

  params->csv_parser = g_new(struct csv_parser, 1);
  if (csv_init(params->csv_parser, options) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(1);
  }

//  reader->file = fopen(reader->trace_path, "rb");
//  if (reader->file == 0) {
//    ERROR("Failed to open %s: %s\n", reader->trace_path, strerror(errno));
//    exit(1);
//  }

  params->current_field_counter = 1;
  params->already_got_req = false;
  params->reader_end = false;

  /* if we setup something here, then we must setup in the reset_reader func */
  if (init_params->delimiter) {
    csv_set_delim(params->csv_parser, init_params->delimiter);
    params->delim = init_params->delimiter;
  }

  if (init_params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_line_ending(reader, &line_end, &line_len);
    reader->mmap_offset = (char *) line_end - reader->mapped_file;
    params->has_header = init_params->has_header;
  }
}

int csv_read_one_element(reader_t *const reader, request_t *const req) {
  csv_params_t *params = reader->reader_params;

  params->req_pointer = req;
  params->already_got_req = false;

  if (params->reader_end) {
    req->valid = false;
    return 1;
  }

  char *line_end = NULL;
  size_t line_len;
  bool end = find_line_ending(reader, &line_end, &line_len);
  /* csv reader needs line ends with CRLF */
  if (line_end - reader->mapped_file < reader->file_size)
    line_len += 1;

  if ((size_t) csv_parse(params->csv_parser,
                         reader->mapped_file + reader->mmap_offset,
                         line_len, csv_cb1, csv_cb2, reader) != line_len)
    WARNING("parsing csv file error: %s\n",
            csv_strerror(csv_error(params->csv_parser)));

  reader->mmap_offset = (char *) line_end - reader->mapped_file;

  if (end)
    params->reader_end = true;

  if (!params->already_got_req) { // didn't read in trace obj_id
    if (params->reader_end)
      csv_fini(params->csv_parser, csv_cb1, csv_cb2, reader);
    else {
      ERROR("parsing csv file, current mmap_offset %lu, next string %s\n",
            (unsigned long) reader->mmap_offset,
            (char *) (reader->mapped_file + reader->mmap_offset));
      abort();
    }
  }
  return 0;
}

guint64 csv_skip_N_elements(reader_t *const reader, const guint64 N) {
  /* this function skips the next N requests,
   * on success, return N,
   * on failure, return the number of requests skipped
   */
  csv_params_t *params = reader->reader_params;

  csv_free(params->csv_parser);
  csv_init(params->csv_parser, CSV_APPEND_NULL);

  if (params->delim)
    csv_set_delim(params->csv_parser, params->delim);

  params->reader_end = false;
  return 0;
}

void csv_reset_reader(reader_t *reader) {
  csv_params_t *params = reader->reader_params;

//  fseek(reader->file, 0L, SEEK_SET);

  csv_free(params->csv_parser);
  csv_init(params->csv_parser, CSV_APPEND_NULL);
  if (params->delim)
    csv_set_delim(params->csv_parser, params->delim);

  if (params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_line_ending(reader, &line_end, &line_len);
    reader->mmap_offset = (char *) line_end - reader->mapped_file;
  }
  params->reader_end = false;
}

void csv_set_no_eof(reader_t *reader) {
  csv_params_t *params = reader->reader_params;
  params->reader_end = false;
}

#ifdef __cplusplus
}
#endif

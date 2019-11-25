//
//  csvReader.c
//  mimircache
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../../include/mimircache/csvReader.h"
#include "../../include/mimircache/logging.h"
#include "libcsv.h"


#ifdef __cplusplus
extern "C"
{
#endif


static inline void csv_cb1(void *s, size_t len, void *data) {
  /* call back for csv field end */

  reader_t *reader = (reader_t *) data;
  csv_params_t *params = reader->reader_params;
  request_t *req = params->req_pointer;

  if (params->current_column_counter == params->label_column) {
    // this field is the request obj_id
    if (reader->base->obj_id_type == OBJ_ID_NUM){
//      printf("current str %s - len %lu - len %lu\n", s, strlen(s), len);
        // len is not correct for the last request for some reason
        req->obj_id_ptr = GSIZE_TO_POINTER(str_to_gsize((char*)s, strlen(s)));
    } else {
      if (len >= MAX_OBJ_ID_LEN)
        len = MAX_OBJ_ID_LEN - 1;
      strncpy((char *) (req->obj_id_ptr), (char *) s, len);
      ((char *) (req->obj_id_ptr))[len] = 0;
    }
    params->already_got_req = TRUE;
  } else if (params->current_column_counter == params->real_time_column) {
    // why this is not a problem because s should not be null terminated
    req->real_time = (guint64) atoll((char *) s);
  } else if (params->current_column_counter == params->op_column) {
    fprintf(stderr, "currently operation column is not supported\n");
  } else if (params->current_column_counter == params->size_column) {
    req->size = (guint64) atoll((char *) s);
  } else if (params->current_column_counter == params->traceID_column) {
    req->traceID = (unsigned char) *((char *) s);
  }

  params->current_column_counter++;
}

static inline void csv_cb2(int c, void *data) {
  /* call back for csv row end */

  reader_t *reader = (reader_t *) data;
  csv_params_t *params = reader->reader_params;
  params->current_column_counter = 1;

  /* move the following code to csv_cb1 after detecting obj_id,
   * because putting here will cause a bug when there is no new line at the
   * end of file, then the last line will have an incorrect reference number
   */

}


csvReader_init_params *new_csvReader_init_params(gint label_column,
                                                 gint op_column,
                                                 gint real_time_column,
                                                 gint size_column,
                                                 gboolean has_header,
                                                 unsigned char delimiter,
                                                 gint traceID_column) {

  csvReader_init_params *init_params = g_new0(csvReader_init_params, 1);
  init_params->label_column = label_column;
  init_params->op_column = op_column;
  init_params->real_time_column = real_time_column;
  init_params->size_column = size_column;
  init_params->has_header = has_header;
  init_params->delimiter = delimiter;
  init_params->traceID_column = traceID_column;
  return init_params;
}


void csv_setup_Reader(const char *const file_loc,
                      reader_t *const reader,
                      const csvReader_init_params *const init_params) {
  unsigned char options = CSV_APPEND_NULL;
  reader->reader_params = g_new0(csv_params_t, 1);
  csv_params_t *params = reader->reader_params;

  /* passed in init_params needs to be saved within reader, to faciliate clone and free */
  reader->base->init_params = g_new(csvReader_init_params, 1);
  memcpy(reader->base->init_params, init_params, sizeof(csvReader_init_params));

  params->csv_parser = g_new(struct csv_parser, 1);
  if (csv_init(params->csv_parser, options) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(1);
  }

  reader->base->file = fopen(file_loc, "rb");
  if (reader->base->file == 0) {
    ERROR("Failed to open %s: %s\n", file_loc, strerror(errno));
    exit(1);
  }

  params->current_column_counter = 1;
  params->op_column = init_params->op_column;
  params->real_time_column = init_params->real_time_column;
  params->size_column = init_params->size_column;
  params->label_column = init_params->label_column;
  params->already_got_req = FALSE;
  params->reader_end = FALSE;


  /* if we setup something here, then we must setup in the reset_reader func */
  if (init_params->delimiter) {
    csv_set_delim(params->csv_parser, init_params->delimiter);
    params->delim = init_params->delimiter;
  }

  if (init_params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_line_ending(reader, &line_end, &line_len);
    reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
    params->has_header = init_params->has_header;
  }
}


void csv_read_one_element(reader_t *const reader, request_t *const c) {
  csv_params_t *params = reader->reader_params;

  params->req_pointer = c;
  params->already_got_req = FALSE;

  if (params->reader_end) {
    c->valid = FALSE;
    return;
  }

  char *line_end = NULL;
  size_t line_len;
  gboolean end = find_line_ending(reader, &line_end, &line_len);
  line_len++;    // because line_len does not include LFCR

  if ((long) csv_parse(params->csv_parser,
                       reader->base->mapped_file + reader->base->mmap_offset,
                       line_len, csv_cb1, csv_cb2, reader) != line_len) WARNING("in parsing csv file: %s\n",
                                                                                csv_strerror(
                                                                                  csv_error(params->csv_parser)));

  reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;

  if (end)
    params->reader_end = TRUE;

  if (!params->already_got_req) {       // didn't read in trace obj_id
    if (params->reader_end)
      csv_fini(params->csv_parser, csv_cb1, csv_cb2, reader);
    else {
      ERROR("in parsing csv file, current mmap_offset %lu, next string %s\n",
            (unsigned long) reader->base->mmap_offset,
            (char *) (reader->base->mapped_file + reader->base->mmap_offset));
      abort();
    }
  }
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

  params->reader_end = FALSE;
  return 0;
}


void csv_reset_reader(reader_t *reader) {
  csv_params_t *params = reader->reader_params;

  fseek(reader->base->file, 0L, SEEK_SET);

  csv_free(params->csv_parser);
  csv_init(params->csv_parser, CSV_APPEND_NULL);
  if (params->delim)
    csv_set_delim(params->csv_parser, params->delim);

  if (params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_line_ending(reader, &line_end, &line_len);
    reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
  }
  params->reader_end = FALSE;
}


void csv_set_no_eof(reader_t *reader) {
  csv_params_t *params = reader->reader_params;
  params->reader_end = FALSE;
}


#ifdef __cplusplus
}
#endif

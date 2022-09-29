//
//  csvReader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "csv.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "libcsv.h"
#include "readerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief count the number of times char c appears in string str
 *
 * @param str
 * @param c
 * @return int
 */
static int count_occurrence(const char *str, const char c) {
  int count = 0;
  for (int i = 0; i < strlen(str); i++) {
    if (str[i] == c) count++;
  }
  return count;
}

/**
 * @brief read the first line of the trace without changing the state of the
 * reader
 *
 * @param reader
 * @param in_buf
 * @param in_buf_size
 * @return int
 */
static int read_first_line(const reader_t *reader, char *in_buf,
                           const size_t in_buf_size) {
  FILE *ifile = fopen(reader->trace_path, "r");
  char *buf = NULL;
  size_t n = 0;
  size_t read_size = getline(&buf, &n, ifile);

  if (in_buf_size < read_size) {
    WARN(
        "in_buf_size %zu is smaller than the first line size %zu, "
        "the first line will be truncated",
        in_buf_size, read_size);
  }

  read_size = read_size > in_buf_size ? in_buf_size : read_size;

  /* + 1 to copy the null terminator */
  memcpy(in_buf, buf, read_size + 1);

  fclose(ifile);
  free(buf);

  return read_size;
}

/**
 * @brief detect delimiter in the csv trace
 * it uses a simple algorithm: count the occurrence of each possible delimiter
 * in the first line, the delimiter with the most occurrence is the delimiter
 * we support 5 possible delimiters: '\t', ',', '|', ':'
 *
 * @param reader
 * @return char
 */
static char csv_detect_delimiter(const reader_t *reader) {
  char first_line[1024] = {0};
  int _buf_size = read_first_line(reader, first_line, 1024);

  char possible_delims[4] = {'\t', ',', '|', ':'};
  int possible_delim_counts[4] = {0, 0, 0, 0};
  int max_count = 0;
  char delimiter = 0;

  for (int i = 0; i < 4; i++) {
    // (5-i) account for the commonality of the delimiters
    possible_delim_counts[i] =
        count_occurrence(first_line, possible_delims[i]) * (4 - i);
    if (possible_delim_counts[i] > max_count) {
      max_count = possible_delim_counts[i];
      delimiter = possible_delims[i];
    }
  }

  assert(delimiter != 0);
  static bool has_printed = false;
  if (!has_printed) {
    INFO("detect csv delimiter %c\n", delimiter);
    has_printed = true;
  }

  return delimiter;
}

/**
 * @brief detect whether the csv trace has header,
 * it uses a simple algorithm: compare the number of digits and letters in the
 * first line, if there are more digits than letters, then the first line is not
 * header, otherwise, the first line is header
 *
 * @param reader
 * @return true
 * @return false
 */
static bool csv_detect_header(const reader_t *reader) {
  char first_line[1024] = {0};
  int buf_size = read_first_line(reader, first_line, 1024);
  assert(buf_size < 1024);

  bool has_header = true;

  int n_digit = 0, n_af = 0, n_letter = 0;
  for (int i = 0; i < buf_size; i++) {
    if (isdigit(first_line[i])) {
      n_digit += 1;
    } else if (isalpha(first_line[i])) {
      n_letter += 1;
      if ((first_line[i] <= 'f' && first_line[i] >= 'a') ||
          (first_line[i] <= 'F' && first_line[i] >= 'A')) {
        /* a-f can be hex number */
        n_af += 1;
      }
    } else {
      ;
    }
  }

  if (n_digit > n_letter) {
    has_header = false;
  } else if (n_digit < n_letter - n_af) {
    has_header = true;
  }

  static bool has_printed = false;
  if (!has_printed) {
    INFO("detect csv trace has header %d\n", has_header);
    has_printed = true;
  }

  return has_header;
}

/**
 * @brief   call back for csv field end
 *
 * @param s     the string of the field
 * @param len   length of the string
 * @param data  user passed data: reader_t*
 */
static inline void csv_cb1(void *s, size_t len, void *data) {
  reader_t *reader = (reader_t *)data;
  csv_params_t *params = reader->reader_params;
  reader_init_param_t *init_params = &reader->init_params;
  request_t *req = params->request;

  if (params->curr_field_idx == init_params->obj_id_field) {
    if (reader->obj_id_type == OBJ_ID_NUM) {
      // len is not correct for the last request for some reason
      // req->obj_id = str_to_obj_id((char *) s, len);
      req->obj_id = atoll((char *)s);
    } else {
      if (len >= MAX_OBJ_ID_LEN) {
        len = MAX_OBJ_ID_LEN - 1;
        ERROR("csvReader obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n", len,
              MAX_OBJ_ID_LEN);
        abort();
      }
      char obj_id_str[MAX_OBJ_ID_LEN];
      memcpy(obj_id_str, (char *)s, len);
      obj_id_str[len] = 0;
      req->obj_id = (uint64_t)g_quark_from_string(obj_id_str);
    }
    params->already_got_req = true;
  } else if (params->curr_field_idx == init_params->time_field) {
    // this does not work, because s is not null terminated
    uint64_t ts = (uint64_t)atof((char *)s);
    // we only support 32-bit ts
    assert(ts < UINT32_MAX);
    req->real_time = ts;
  } else if (params->curr_field_idx == init_params->op_field) {
    fprintf(stderr, "currently operation column is not supported\n");
  } else if (params->curr_field_idx == init_params->obj_size_field) {
    req->obj_size = (uint64_t)atoll((char *)s);
  }

  params->curr_field_idx++;
}

static inline void csv_cb2(int c, void *data) {
  /* call back for csv row end */

  reader_t *reader = (reader_t *)data;
  csv_params_t *params = reader->reader_params;
  params->curr_field_idx = 1;

  /* move the following code to csv_cb1 after detecting obj_id,
   * because putting here will cause a bug when there is no new line at the
   * end of file, then the last line will have an incorrect reference number
   */
}

void csv_setup_reader(reader_t *const reader) {
  unsigned char options = CSV_APPEND_NULL;
  reader->trace_format = TXT_TRACE_FORMAT;
  reader_init_param_t *init_params = &reader->init_params;

  reader->reader_params = (csv_params_t *)malloc(sizeof(csv_params_t));
  csv_params_t *params = reader->reader_params;

  params->curr_field_idx = 1;
  params->already_got_req = false;
  params->reader_end = false;

  params->time_field_idx = init_params->time_field;
  params->obj_id_field_idx = init_params->obj_id_field;
  params->size_field_idx = init_params->obj_size_field;
  params->csv_parser = (struct csv_parser *)malloc(sizeof(struct csv_parser));

  if (csv_init(params->csv_parser, options) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(1);
  }

  reader->file = fopen(reader->trace_path, "rb");
  if (reader->file == 0) {
    ERROR("Failed to open %s: %s\n", reader->trace_path, strerror(errno));
    exit(1);
  }

  /* if we setup something here, then we must setup in the reset_reader func */
  if (init_params->delimiter == '\0') {
    char delim = csv_detect_delimiter(reader);
    params->delimiter = delim;
  } else {
    params->delimiter = init_params->delimiter;
  }
  csv_set_delim(params->csv_parser, params->delimiter);

  if (!init_params->has_header_set) {
    params->has_header = csv_detect_header(reader);
  } else {
    params->has_header = init_params->has_header;
  }
  if (params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_end_of_line(reader, &line_end, &line_len);
    reader->mmap_offset = (char *)line_end - reader->mapped_file;
  }
}

int csv_read_one_req(reader_t *const reader, request_t *const req) {
  csv_params_t *params = reader->reader_params;

  params->request = req;
  params->already_got_req = false;

  if (params->reader_end) {
    req->valid = false;
    return 1;
  }

  char *line_end = NULL;
  size_t line_len;
  bool end = find_end_of_line(reader, &line_end, &line_len);
  line_len++;  // because line_len does not include LFCR

  if ((size_t)csv_parse(params->csv_parser,
                        reader->mapped_file + reader->mmap_offset, line_len,
                        csv_cb1, csv_cb2, reader) != line_len)
    WARN("parsing csv file error: %s\n",
         csv_strerror(csv_error(params->csv_parser)));

  reader->mmap_offset = (char *)line_end - reader->mapped_file;

  if (end) params->reader_end = true;

  if (!params->already_got_req) {  // didn't read in trace obj_id
    if (params->reader_end)
      csv_fini(params->csv_parser, csv_cb1, csv_cb2, reader);
    else {
      ERROR("parsing csv file, current mmap_offset %lu, file size %zu\n",
            (unsigned long)reader->mmap_offset, reader->file_size);
      abort();
    }
  }
  return 0;
}

uint64_t csv_skip_N_elements(reader_t *const reader, const uint64_t N) {
  /* this function skips the next N requests,
   * on success, return N,
   * on failure, return the number of requests skipped
   */
  csv_params_t *params = reader->reader_params;

  csv_free(params->csv_parser);
  csv_init(params->csv_parser, CSV_APPEND_NULL);

  if (params->delimiter) csv_set_delim(params->csv_parser, params->delimiter);

  params->reader_end = false;
  return 0;
}

void csv_reset_reader(reader_t *reader) {
  csv_params_t *params = reader->reader_params;

  fseek(reader->file, 0L, SEEK_SET);

  csv_free(params->csv_parser);
  csv_init(params->csv_parser, CSV_APPEND_NULL);
  if (params->delimiter) csv_set_delim(params->csv_parser, params->delimiter);

  if (params->has_header) {
    char *line_end = NULL;
    size_t line_len = 0;
    find_end_of_line(reader, &line_end, &line_len);
    reader->mmap_offset = (char *)line_end - reader->mapped_file;
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

//
//  csvReader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "../readerInternal.h"
#include "dataStructure/hash/hash.h"
#include "libCacheSim/macro.h"
#include "libcsv.h"

#ifdef __cplusplus
extern "C" {
#endif

// to suppress the warning of getline
// ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/**
 * @brief count the number of times char c appears in string str
 *
 * @param str
 * @param c
 * @return int
 */
static int count_occurrence(const char *str, const char c) {
  int count = 0;
  for (int i = 0; i < (int)strlen(str); i++) {
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
  read_first_line(reader, first_line, 1024);

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
 * @brief check whether the trace uses the given delimiter by making sure
 *        the delimiter is in each line
 *
 * @param reader
 * @param delimiter
 * @return bool
 */
bool check_delimiter(const reader_t *reader, char delimiter) {
  FILE *ifile = fopen(reader->trace_path, "r");
  char *buf = NULL;
  bool is_delimiter_correct = true;
  size_t n = 0;
  ssize_t n_read = getline(&buf, &n, ifile);
  DEBUG_ASSERT(n_read != -1);

#define N_TEST 1024
  for (int i = 0; i < N_TEST; i++) {
    if (strchr(buf, delimiter) == NULL) {
      is_delimiter_correct = false;
      break;
    }
  }
#undef N_TEST

  fclose(ifile);
  free(buf);

  return is_delimiter_correct;
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
  csv_params_t *csv_params = reader->reader_params;
  request_t *req = csv_params->request;
  char *end;

  if (csv_params->curr_field_idx == csv_params->obj_id_field_idx) {
    if (reader->obj_id_is_num) {
      req->obj_id = strtoull((char *)s, &end, 0);
      if (req->obj_id == 0 && s == end) {
        WARN("object id is not numeric: \"%s\"\n", (char *)s);
      }
    } else {
      if (!reader->obj_id_is_num_set) {
        if (is_str_num((char *)s, len)) {
          csv_params->n_obj_id_is_num++;
        } else {
          csv_params->n_obj_id_is_not_num++;
        }
        int n_req =
            csv_params->n_obj_id_is_num + csv_params->n_obj_id_is_not_num;
        if (n_req > 20000) {
          if (csv_params->n_obj_id_is_num > (double)n_req * 0.99) {
            ERROR(
                "detect obj_id is numeric, please specify -t "
                "'obj-id-is-num=1'\n");
          }
        }
      }
      // req->obj_id = (uint64_t)g_quark_from_string(s);
      req->obj_id = (uint64_t)get_hash_value_str((char *)s, len);
    }
  } else if (csv_params->curr_field_idx == csv_params->time_field_idx) {
    // int64_t ts = (int64_t)atof((char *)s);
    int64_t ts = (int64_t)strtod((char *)s, NULL);
    req->clock_time = ts;
  } else if (csv_params->curr_field_idx == csv_params->obj_size_field_idx) {
    req->obj_size = (int64_t)strtoll((char *)s, &end, 0);
    if (req->obj_size == 0 && end == s) {
      WARN("csvReader obj_size is not a number: \"%s\"\n", (char *)s);
    }
  } else if (csv_params->curr_field_idx == csv_params->op_field_idx) {
    if (strncasecmp((char *)s, "read", len) == 0) {
      req->op = OP_READ;
    } else if (strncasecmp((char *)s, "write", len) == 0) {
      req->op = OP_WRITE;
    } else if (strncasecmp((char *)s, "get", len) == 0) {
      req->op = OP_GET;
    } else if (strncasecmp((char *)s, "set", len) == 0) {
      req->op = OP_SET;
    } else if (strncasecmp((char *)s, "delete", len) == 0) {
      req->op = OP_DELETE;
    } else {
      WARN("unknown operation: \"%s\"\n", (char *)s);
    }
  } else if (csv_params->curr_field_idx == csv_params->ttl_field_idx) {
    req->ttl = (uint32_t)strtoul((char *)s, &end, 0);
  } else if (csv_params->curr_field_idx == csv_params->cnt_field_idx) {
    reader->n_req_left = (uint64_t)strtoull((char *)s, &end, 0) - 1;
  } else if (csv_params->curr_field_idx == csv_params->tenant_field_idx) {
    req->tenant_id = (int32_t)strtoul((char *)s, &end, 0);
  } else {
    for (int i = 0; i < csv_params->n_feature_fields; i++) {
      if (csv_params->curr_field_idx == csv_params->feature_fields[i]) {
        req->features[i] = (int32_t)strtoul((char *)s, &end, 0);
      }
    }
    req->n_features = csv_params->n_feature_fields;
  }
  csv_params->curr_field_idx++;
}

/**
 * @brief   call back for csv row end
 *
 * @param c     the character that ends the row
 * @param data  user passed data: reader_t*
 */
static inline void csv_cb2(int c, void *data) {
  reader_t *reader = (reader_t *)data;
  csv_params_t *csv_params = reader->reader_params;
  csv_params->curr_field_idx = 1;
}

/**
 * @brief setup a csv reader
 *
 * @param reader
 */
void csv_setup_reader(reader_t *const reader) {
  unsigned char options = CSV_APPEND_NULL;
  reader->trace_format = TXT_TRACE_FORMAT;
  reader_init_param_t *init_params = &reader->init_params;

  csv_params_t *csv_params = (csv_params_t *)malloc(sizeof(csv_params_t));
  memset(csv_params, 0, sizeof(csv_params_t));
  reader->reader_params = csv_params;
  csv_params->curr_field_idx = 1;

  csv_params->time_field_idx = init_params->time_field;
  csv_params->obj_id_field_idx = init_params->obj_id_field;
  csv_params->obj_size_field_idx = init_params->obj_size_field;
  csv_params->op_field_idx = init_params->op_field;
  csv_params->ttl_field_idx = init_params->ttl_field;
  csv_params->cnt_field_idx = init_params->cnt_field;
  csv_params->tenant_field_idx = init_params->tenant_field;
  csv_params->n_feature_fields = init_params->n_feature_fields;
  for (int i = 0; i < csv_params->n_feature_fields; i++) {
    csv_params->feature_fields[i] = init_params->feature_fields[i];
  }

  csv_params->csv_parser =
      (struct csv_parser *)malloc(sizeof(struct csv_parser));
  csv_params->n_obj_id_is_num = 0;
  csv_params->n_obj_id_is_not_num = 0;

  if (csv_init(csv_params->csv_parser, options) != 0) {
    fprintf(stderr, "Failed to initialize csv parser\n");
    exit(1);
  }

  /* if we setup something here, then we must setup in the reset_reader func */
  if (init_params->delimiter == '\0') {
    char delim = csv_detect_delimiter(reader);
    csv_params->delimiter = delim;
  } else {
    csv_params->delimiter = init_params->delimiter;
  }
  csv_set_delim(csv_params->csv_parser, csv_params->delimiter);

  if (!init_params->has_header_set) {
    csv_params->has_header = csv_detect_header(reader);
  } else {
    csv_params->has_header = init_params->has_header;
  }
  if (csv_params->has_header) {
    ssize_t read_size =
        getline(&reader->line_buf, &reader->line_buf_size, reader->file);
    reader->trace_start_offset = read_size;
  }
}

/**
 * @brief read one request from a csv file
 *
 * @param reader
 * @param req
 * @return int
 */
int csv_read_one_req(reader_t *const reader, request_t *const req) {
  csv_params_t *csv_params = reader->reader_params;
  struct csv_parser *csv_parser = csv_params->csv_parser;
  char **line_buf_ptr = &reader->line_buf;
  size_t *line_buf_size_ptr = &reader->line_buf_size;

  csv_params->request = req;
  DEBUG_ASSERT(csv_params->curr_field_idx == 1);

  ssize_t read_size = getline(line_buf_ptr, line_buf_size_ptr, reader->file);
  if (read_size == -1) {
    req->valid = false;
    return 1;
  }

  if ((ssize_t)csv_parse(csv_parser, *line_buf_ptr, read_size, csv_cb1, csv_cb2,
                         reader) != read_size) {
    WARN("parsing csv file error: %s\n",
         csv_strerror(csv_error(csv_params->csv_parser)));
  }

  csv_fini(csv_params->csv_parser, csv_cb1, csv_cb2, reader);

  if (req->obj_size == 0 && reader->ignore_size_zero_req) {
    if (reader->read_direction == READ_FORWARD) {
      return csv_read_one_req(reader, req);
    } else {
      return read_one_req_above(reader, req);
    }
  }

  if (reader->n_req_left > 0) reader->last_req_clock_time = req->clock_time;

  return 0;
}

void csv_reset_reader(reader_t *reader) {
  csv_params_t *csv_params = reader->reader_params;

  fseek(reader->file, 0L, SEEK_SET);

  csv_free(csv_params->csv_parser);
  csv_init(csv_params->csv_parser, CSV_APPEND_NULL);
  csv_params->n_obj_id_is_num = 0;
  csv_params->n_obj_id_is_not_num = 0;
  if (csv_params->delimiter)
    csv_set_delim(csv_params->csv_parser, csv_params->delimiter);

  if (csv_params->has_header) {
    int read_size =
        getline(&reader->line_buf, &reader->line_buf_size, reader->file);
    reader->trace_start_offset = read_size;
  }
}

#ifdef __cplusplus
}
#endif

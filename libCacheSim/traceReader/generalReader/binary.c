//
//  read a binary trace specified using format string,
//  the format string follows Python struct module, starts with endien and
//  followed by the format, this only supports little-endian with limited format
//  types, this supports various char and int types, float and double are
//  supported, but converted to int64_t, string is not supported, multiple same
//  format cannot be combined, for example, "II" cannot be written as "2I", but
//  rather explicitly write "II"
//
//
//  binaryReader.c
//  libCacheSim
//
//  Created by Juncheng on 2/28/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include <string.h>

#include "../readerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

static int cal_offset(const char *format_str, int field_idx) {
  int offset = 0;
  for (int i = 0; i < field_idx - 1; i++) {
    offset += format_to_size(format_str[i]);
  }
  return offset;
}

int binaryReader_setup(reader_t *const reader) {
  reader->trace_type = BIN_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 0;

  reader->reader_params = (binary_params_t *)malloc(sizeof(binary_params_t));
  binary_params_t *params = (binary_params_t *)reader->reader_params;
  reader_init_param_t *init_params = &reader->init_params;
  memset(params, 0, sizeof(binary_params_t));

  /* begin parsing input params and fmt */
  if (reader->init_params.binary_fmt_str == NULL) {
    ERROR("binaryReader_setup: fmt_str is NULL\n");
  }

  if (reader->init_params.binary_fmt_str[0] != '<') {
    ERROR(
        "binary trace only supports little endian and format string should "
        "start with <\n");
  }

  // skip the first '<'
  params->fmt_str = strdup(init_params->binary_fmt_str + 1);
  const char *fmt_str = params->fmt_str;

  params->n_fields = strlen(fmt_str);

  params->obj_id_field_idx = reader->init_params.obj_id_field;
  params->obj_id_format = fmt_str[params->obj_id_field_idx - 1];
  params->obj_id_offset = cal_offset(fmt_str, params->obj_id_field_idx);

  params->time_field_idx = reader->init_params.time_field;
  if (params->time_field_idx > 0) {
    params->time_format = fmt_str[params->time_field_idx - 1];
    params->time_offset = cal_offset(fmt_str, params->time_field_idx);
  } else {
    params->time_format = '\0';
    params->time_offset = -1;
  }

  params->obj_size_field_idx = reader->init_params.obj_size_field;
  if (params->obj_size_field_idx > 0) {
    params->obj_size_format = fmt_str[params->obj_size_field_idx - 1];
    params->obj_size_offset = cal_offset(fmt_str, params->obj_size_field_idx);
  } else {
    params->obj_size_format = '\0';
    params->obj_size_offset = -1;
  }

  params->op_field_idx = reader->init_params.op_field;
  if (params->op_field_idx > 0) {
    params->op_format = fmt_str[params->op_field_idx - 1];
    params->op_offset = cal_offset(fmt_str, params->op_field_idx);
  } else {
    params->op_format = '\0';
    params->op_offset = -1;
  }

  params->ttl_field_idx = reader->init_params.ttl_field;
  if (params->ttl_field_idx > 0) {
    params->ttl_format = fmt_str[params->ttl_field_idx - 1];
    params->ttl_offset = cal_offset(fmt_str, params->ttl_field_idx);
  } else {
    params->ttl_format = '\0';
    params->ttl_offset = -1;
  }

  params->next_access_vtime_field_idx =
      reader->init_params.next_access_vtime_field;
  if (params->next_access_vtime_field_idx > 0) {
    params->next_access_vtime_format =
        fmt_str[params->next_access_vtime_field_idx - 1];
    params->next_access_vtime_offset =
        cal_offset(fmt_str, params->next_access_vtime_field_idx);
  } else {
    params->next_access_vtime_format = '\0';
    params->next_access_vtime_offset = -1;
  }

  reader->item_size = cal_offset(fmt_str, params->n_fields + 1);
  params->item_size = reader->item_size;
  if (reader->item_size == 0) {
    ERROR("binaryReader_setup: item_size is 0, fmt \"%s\", %d fields\n",
          fmt_str, params->n_fields);
  }

  ssize_t data_region_size = reader->file_size - reader->trace_start_offset;
  if (data_region_size % reader->item_size != 0) {
    WARN(
        "trace file size %lu - %lu is not multiple of item size %lu, mod %lu\n",
        (unsigned long)reader->file_size,
        (unsigned long)reader->trace_start_offset,
        (unsigned long)reader->item_size,
        (unsigned long)reader->file_size % reader->item_size);
  }

  reader->n_total_req = (uint64_t)data_region_size / (reader->item_size);

  char output[1024];
  int n = snprintf(
      output, 1024,
      "binary fmt %s, item size %d, "
      "obj_id_field_idx %d, size %d, offset %d",
      params->fmt_str, (int)reader->item_size, params->obj_id_field_idx,
      format_to_size(params->obj_id_format), params->obj_id_offset);

  if (params->time_field_idx > 0) {
    n += snprintf(output + n, 1024 - n, ", time_field %d,%d,%d",
                  params->time_field_idx, format_to_size(params->time_format),
                  params->time_offset);
  }
  if (params->obj_size_field_idx > 0) {
    n += snprintf(output + n, 1024 - n, ", obj_size_field %d,%d,%d",
                  params->obj_size_field_idx,
                  format_to_size(params->obj_size_format),
                  params->obj_size_offset);
  }
  if (params->op_field_idx > 0) {
    n += snprintf(output + n, 1024 - n, ", op_field %d,%d,%d",
                  params->op_field_idx, format_to_size(params->op_format),
                  params->op_offset);
  }
  if (params->ttl_field_idx > 0) {
    n += snprintf(output + n, 1024 - n, ", ttl_field %d,%d,%d",
                  params->ttl_field_idx, format_to_size(params->ttl_format),
                  params->ttl_offset);
  }
  if (params->next_access_vtime_field_idx > 0) {
    n += snprintf(output + n, 1024 - n, ", next_access_vtime_field %d,%d,%d",
                  params->next_access_vtime_field_idx,
                  format_to_size(params->next_access_vtime_format),
                  params->next_access_vtime_offset);
  }
  DEBUG("%s\n", output);

  return 0;
}

/**
 * @brief read one piece of data at src according to the format
 * return the value as int64_t
 * this limits our format support to int types
 *
 * @param src
 * @param format
 */
static inline int64_t read_data(char *src, char format) {
  ;

  switch (format) {
    case 'b':
    case 'B':
    case 'c':
      return (int64_t)(*(int8_t *)src);
    case 'h':
    case 'H':
      return (int64_t)(*(int16_t *)src);
    case 'i':
    case 'l':
    case 'I':
    case 'L':
      return (int64_t)(*(int32_t *)src);
    case 'q':
    case 'Q':
      return (int64_t)(*(int64_t *)src);
    case 'f':
      return (int64_t)(*(float *)src);
    case 'd':
      return (int64_t)(*(double *)src);
    default:
      ERROR("DO NOT recognize given format character: %c\n", format);
      break;
  }
}

int binary_read_one_req(reader_t *reader, request_t *req) {
  binary_params_t *params = (binary_params_t *)reader->reader_params;

  char *start = (reader->mapped_file + reader->mmap_offset);

  /* read object id */
  req->obj_id = read_data(start + params->obj_id_offset, params->obj_id_format);

  /* read time */
  if (params->time_field_idx > 0) {
    req->clock_time =
        read_data(start + params->time_offset, params->time_format);
  }

  /* read object size */
  if (params->obj_size_field_idx > 0) {
    req->obj_size =
        read_data(start + params->obj_size_offset, params->obj_size_format);
  }

  /* read operation */
  if (params->op_field_idx > 0) {
    req->op = read_data(start + params->op_offset, params->op_format);
  }

#ifdef ENABLE_TTL
  /* read ttl */
  if (params->ttl_field_idx > 0) {
    req->ttl = read_data(start + params->ttl_offset, params->ttl_format);
  }
#endif

  /* read next access vtime */
  if (params->next_access_vtime_field_idx > 0) {
    req->next_access_vtime = read_data(start + params->next_access_vtime_offset,
                                       params->next_access_vtime_format);
  }

  (reader->mmap_offset) += reader->item_size;
  return 0;
}

#ifdef __cplusplus
}
#endif

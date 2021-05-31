//
//  binaryReader.c
//  libCacheSim
//
//  Created by Juncheng on 2/28/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "binary.h"

#ifdef __cplusplus
extern "C"
{
#endif

int binaryReader_setup(reader_t *const reader) {

  reader->trace_type = BIN_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 0;

  reader->reader_params = g_new0(binary_params_t, 1);
  reader_init_param_t *init_params = &reader->init_params;
  binary_params_t *params = (binary_params_t *) reader->reader_params;


  /* begin parsing input params and fmt */
  const char *fmt_str = reader->init_params.binary_fmt;
  // ignore the first few characters related to endien
  while (!((*fmt_str >= '0' && *fmt_str <= '9')
      || (*fmt_str >= 'a' && *fmt_str <= 'z')
      || (*fmt_str >= 'A' && *fmt_str <= 'Z'))) {
    fmt_str++;
  }

  guint count = 0, last_count_sum = 0;
  gint count_sum = 0;
  guint size = 0;

  while (*fmt_str) {
    count = 0;
    while (*fmt_str >= '0' && *fmt_str <= '9') {
      count *= 10;
      count += *fmt_str - '0';
      fmt_str++;
    }
    if (count == 0)
      count = 1;      // does not have a number prepend format character
    last_count_sum = count_sum;
    count_sum += count;

    switch (*fmt_str) {
      case 'b':
      case 'B':
      case 'c':
      case '?':size = 1;
        break;

      case 'h':
      case 'H':size = 2;
        break;

      case 'i':
      case 'I':
      case 'l':
      case 'L':size = 4;
        break;

      case 'q':
      case 'Q':size = 8;
        break;

      case 'f':size = 4;
        break;

      case 'd':size = 8;
        break;

      case 's':size = 1;
        // for string, it should be only one
        count_sum = count_sum - count + 1;
        break;

      default: ERROR("can NOT recognize given format character: %c\n",
                     *fmt_str);
        break;
    }

    if (init_params->obj_id_field != 0 && params->obj_id_len == 0
        && init_params->obj_id_field <= count_sum) {
      params->obj_id_field = (int) reader->item_size +
          size * (init_params->obj_id_field - last_count_sum - 1);
      params->obj_id_len = *fmt_str == 's' ? count : 1;
      params->obj_id_type = *fmt_str;
      // important! update data obj_id_type here
      reader->obj_id_type = *fmt_str == 's' ? OBJ_ID_STR : OBJ_ID_NUM;
    }

    if (init_params->op_field != 0 && params->op_len == 0
        && init_params->op_field <= count_sum) {
      params->op_field = (int) reader->item_size +
          size * (init_params->op_field - last_count_sum - 1);
      params->op_len = *fmt_str == 's' ? count : 1;
      params->op_type = *fmt_str;
    }

    if (init_params->real_time_field != 0 && params->real_time_len == 0
        && init_params->real_time_field <= count_sum) {
      params->real_time_field = (int) reader->item_size +
          size * (init_params->real_time_field - last_count_sum - 1);
      params->real_time_len = *fmt_str == 's' ? count : 1;
      if (params->real_time_len > 4)
        WARNING("only support timestamp in uint32_t\n");
      params->real_time_type = *fmt_str;
    }

    if (init_params->obj_size_field != 0 && params->obj_size_len == 0
        && init_params->obj_size_field <= count_sum) {
      params->obj_size_field = (int) reader->item_size +
          size * (init_params->obj_size_field - last_count_sum - 1);
      params->obj_size_len  = *fmt_str == 's' ? count : 1;
      params->obj_size_type = *fmt_str;
    }

    if (init_params->ttl_field != 0 && params->ttl_len == 0
        && init_params->ttl_field <= count_sum) {
      params->ttl_field = (int) reader->item_size +
          size * (init_params->ttl_field - last_count_sum - 1);
      params->ttl_len  = *fmt_str == 's' ? count : 1;
      params->ttl_type = *fmt_str;
    }

    if (init_params->extra_field1 != 0 && params->extra_len1 == 0
        && init_params->extra_field1 <= count_sum) {
      params->extra_field1 = (int) reader->item_size +
          size * (init_params->extra_field1 - last_count_sum - 1);
      params->extra_len1  = *fmt_str == 's' ? count : 1;
      params->extra_type1 = *fmt_str;
    }

    if (init_params->extra_field2 != 0 && params->extra_len2 == 0
        && init_params->extra_field2 <= count_sum) {
      params->extra_field2 = (int) reader->item_size +
          size * (init_params->extra_field2 - last_count_sum - 1);
      params->extra_len2  = *fmt_str == 's' ? count : 1;
      params->extra_type2 = *fmt_str;
    }


    reader->item_size += count * size;
    fmt_str++;
  }

  // ASSERTION
  if (init_params->obj_id_field == -1) {
    ERROR("obj_id position cannot be -1\n");
    exit(1);
  }

  if (reader->file_size % reader->item_size != 0) {
    WARNING("trace file size %lu is not multiple of record size %lu, mod %lu\n",
            (unsigned long) reader->file_size,
            (unsigned long) reader->item_size,
            (unsigned long) reader->file_size % reader->item_size);
  }

  reader->n_total_req = (guint64) reader->file_size / (reader->item_size);
  params->num_of_fields = count_sum;

  return 0;
}

/* binary extract extracts the attribute from record, at given pos */
static inline void binary_extract(char *record, int pos, int len,
                                  char type, void *written_to) {

  ;

  switch (type) {
    case 'b':
    case 'B':
    case 'c':
    case '?': WARNING("given obj_id_type %c cannot be used for obj_id or time\n", type);
      break;

    case 'h':
//            *(gint16*)written_to = *(gint16*)(record + pos);
      *(gint64 *) written_to = *(gint16 *) (record + pos);
      break;
    case 'H':
      *(gint64 *) written_to = *(guint16 *) (record + pos);
      break;

    case 'i':
    case 'l':
      *(gint64 *) written_to = *(gint32 *) (record + pos);
      break;

    case 'I':
    case 'L':
      *(gint64 *) written_to = *(guint32 *) (record + pos);
      break;

    case 'q':
      *(gint64 *) written_to = *(gint64 *) (record + pos);
      break;
    case 'Q':
      *(gint64 *) written_to = *(guint64 *) (record + pos);
      break;

    case 'f':
      *(float *) written_to = *(float *) (record + pos);
      *(double *) written_to = *(float *) (record + pos);
      break;

    case 'd':
      *(double *) written_to = *(double *) (record + pos);
      break;

    case 's':
      strncpy((char *) written_to, (char *) (record + pos), len);
      ((char *) written_to)[len] = 0;
      break;

    default: ERROR("DO NOT recognize given format character: %c\n", type);
      break;
  }
}

int binary_read_one_req(reader_t *reader, request_t *req) {
  if (reader->mmap_offset >= reader->n_total_req * reader->item_size) {
    req->valid = false;
    return 1;
  }

  binary_params_t *params = (binary_params_t *) reader->reader_params;

  char *record = (reader->mapped_file + reader->mmap_offset);
  if (params->obj_id_type) {
    binary_extract(record, params->obj_id_field, params->obj_id_len,
                   params->obj_id_type, &(req->obj_id));
  }
  if (params->real_time_type) {
    binary_extract(record, params->real_time_field, params->real_time_len,
                   params->real_time_type, &(req->real_time));
  }
  if (params->obj_size_type) {
    binary_extract(record, params->obj_size_field, params->obj_size_len,
                   params->obj_size_type, &(req->obj_size));
  }
  if (params->op_type) {
    binary_extract(record, params->op_field, params->op_len,
                   params->op_type, &(req->op));
  }
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (params->ttl_type) {
    binary_extract(record, params->ttl_field, params->ttl_len,
                   params->ttl_type, &(req->ttl));
  }
#endif
//  if (params->extra_type1) {
//    binary_extract(record, params->extra_field1, params->extra_len1,
//                   params->extra_type1, &(req->extra_field1));
//  }
//  if (params->extra_type2) {
//    binary_extract(record, params->extra_field2, params->extra_len2,
//                   params->extra_type2, &(req->extra_field2));
//  }


  (reader->mmap_offset) += reader->item_size;
  return 0;
}

#ifdef __cplusplus
}
#endif

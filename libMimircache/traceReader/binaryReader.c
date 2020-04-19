//
//  binaryReader.c
//  libMimircache
//
//  Created by Juncheng on 2/28/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "include/binaryReader.h"

#ifdef __cplusplus
extern "C"
{
#endif


int binaryReader_setup(const char *const filename,
                       reader_t *const reader,
                       const reader_init_param_t *const init_params) {

  /* passed in init_params needs to be saved within reader,
   * to faciliate clone and free */
  reader->base->init_params = g_new(reader_init_param_t, 1);
  memcpy(reader->base->init_params, init_params, sizeof(reader_init_param_t));

  reader->base->trace_type = BIN_TRACE;
  reader->base->item_size = 0;

  reader->reader_params = g_new0(binary_params_t, 1);
  binary_params_t *params = (binary_params_t *) reader->reader_params;


  /* begin parsing input params and fmt */
  const char *fmt_str = init_params->binary_fmt;
  // ignore the first few characters related to endien
  while (!((*fmt_str >= '0' && *fmt_str <= '9') || (*fmt_str >= 'a' && *fmt_str <= 'z') || (*fmt_str >= 'A' && *fmt_str <= 'Z'))) {
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

      default: ERROR("can NOT recognize given format character: %c\n", *fmt_str);
        break;
    }

    if (init_params->obj_id_field != 0 && params->obj_id_len == 0 && init_params->obj_id_field <= count_sum) {
      params->obj_id_field = (gint) reader->base->item_size +
          size * (init_params->obj_id_field - last_count_sum - 1);
      params->obj_id_len = *fmt_str == 's' ? count : 1;
      params->obj_id_type = *fmt_str;
      // important! update data obj_id_type here
      reader->base->obj_id_type = *fmt_str == 's' ? OBJ_ID_STR : OBJ_ID_NUM;
    }

    if (init_params->op_field != 0 && params->op_len == 0 && init_params->op_field <= count_sum) {
      params->op_field = (gint) reader->base->item_size +
          size * (init_params->op_field - last_count_sum - 1);
      params->op_len = *fmt_str == 's' ? count : 1;
      params->op_type = *fmt_str;
    }

    if (init_params->real_time_field != 0 && params->real_time_len == 0 && init_params->real_time_field <= count_sum) {
      params->real_time_field = (gint) reader->base->item_size +
          size * (init_params->real_time_field - last_count_sum - 1);
      params->real_time_len = *fmt_str == 's' ? count : 1;
      params->real_time_type = *fmt_str;
    }

    if (init_params->obj_size_field != 0 && params->obj_size_len == 0 && init_params->obj_size_field <= count_sum) {
      params->obj_size_field = (gint) reader->base->item_size +
          size * (init_params->obj_size_field - last_count_sum - 1);
      params->obj_size_len = size;
      params->obj_size_type = *fmt_str;
    }

    if (init_params->ttl_field != 0 && params->ttl_len == 0 && init_params->ttl_field <= count_sum) {
      params->ttl_field = (gint) reader->base->item_size +
                               size * (init_params->ttl_field - last_count_sum - 1);
      params->ttl_len = size;
      params->ttl_type = *fmt_str;
    }

    reader->base->item_size += count * size;
    fmt_str++;
  }

  // ASSERTION
  if (init_params->obj_id_field == -1) {
    ERROR("obj_id position cannot be -1\n");
    exit(1);
  }

  if (reader->base->file_size % reader->base->item_size != 0) {
    WARNING("trace file size %lu is not multiple of record size %lu, mod %lu\n",
            (unsigned long) reader->base->file_size,
            (unsigned long) reader->base->item_size,
            (unsigned long) reader->base->file_size % reader->base->item_size);
  }

  reader->base->n_total_req = (guint64) reader->base->file_size / (reader->base->item_size);
  params->num_of_fields = count_sum;

  return 0;
}

#ifdef __cplusplus
}
#endif

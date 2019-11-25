//
//  binaryReader.c
//  mimircache
//
//  Created by Juncheng on 2/28/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "../../include/mimircache/binaryReader.h"
#include "../../include/mimircache/logging.h"

#ifdef __cplusplus
extern "C"
{
#endif

binary_init_params_t *new_binaryReader_init_params(gint obj_id_pos,
                                                   gint op_pos,
                                                   gint real_time_pos,
                                                   gint size_pos,
                                                   const char *fmt) {

  binary_init_params_t *init_params = g_new0(binary_init_params_t, 1);
  init_params->obj_id_pos = obj_id_pos;
  init_params->op_pos = op_pos;
  init_params->real_time_pos = real_time_pos;
  init_params->size_pos = size_pos;
  init_params->unused_pos1 = -1;
  init_params->unused_pos2 = -1;
  strcpy(init_params->fmt, fmt);
  return init_params;
}

int binaryReader_setup(const char *const filename,
                       reader_t *const reader,
                       const binary_init_params_t *const init_params) {

  /* passed in init_params needs to be saved within reader,
   * to faciliate clone and free */
  reader->base->init_params = g_new(binary_init_params_t, 1);
  memcpy(reader->base->init_params, init_params, sizeof(binary_init_params_t));

  reader->base->trace_type = BIN_TRACE;
  reader->base->item_size = 0;

  reader->reader_params = g_new0(binary_params_t, 1);
  binary_params_t *params = (binary_params_t *) reader->reader_params;
  strcpy(params->fmt, init_params->fmt);


  /* begin parsing input params and fmt */
  const char *fmt_str = init_params->fmt;
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

    if (init_params->obj_id_pos != 0 && params->obj_id_len == 0 && init_params->obj_id_pos <= count_sum) {
      params->obj_id_pos = (gint) reader->base->item_size +
          size * (init_params->obj_id_pos - last_count_sum - 1);
      params->obj_id_len = *fmt_str == 's' ? count : 1;
      params->obj_id_type = *fmt_str;
      // important! update data obj_id_type here
      reader->base->obj_id_type = *fmt_str == 's' ? OBJ_ID_STR : OBJ_ID_NUM;
    }

    if (init_params->op_pos != 0 && params->op_len == 0 && init_params->op_pos <= count_sum) {
      params->op_pos = (gint) reader->base->item_size +
          size * (init_params->op_pos - last_count_sum - 1);
      params->op_len = *fmt_str == 's' ? count : 1;
      params->op_type = *fmt_str;
    }

    if (init_params->real_time_pos != 0 && params->real_time_len == 0 && init_params->real_time_pos <= count_sum) {
      params->real_time_pos = (gint) reader->base->item_size +
          size * (init_params->real_time_pos - last_count_sum - 1);
      params->real_time_len = *fmt_str == 's' ? count : 1;
      params->real_time_type = *fmt_str;
    }

    if (init_params->size_pos != 0 && params->size_len == 0 && init_params->size_pos <= count_sum) {
      params->size_pos = (gint) reader->base->item_size +
          size * (init_params->size_pos - last_count_sum - 1);
      params->size_len = size;
      params->size_type = *fmt_str;
    }

    if (init_params->unused_pos1 != 0 && params->unused_len1 == 0 && init_params->unused_pos1 <= count_sum) {
      params->unused_pos1 = (gint) reader->base->item_size +
          size * (init_params->unused_pos1 - last_count_sum - 1);
      params->unused_len1 = *fmt_str == 's' ? count : 1;
      params->unused_type1 = *fmt_str;
    }

    if (init_params->unused_pos2 != 0 && params->unused_len2 == 0 && init_params->unused_pos2 <= count_sum) {
      params->unused_pos2 = (gint) reader->base->item_size +
          size * (init_params->unused_pos2 - last_count_sum - 1);
      params->unused_len2 = *fmt_str == 's' ? count : 1;
      params->unused_type2 = *fmt_str;
    }

    reader->base->item_size += count * size;
    fmt_str++;
  }

  // ASSERTION
  if (init_params->obj_id_pos == -1) {
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

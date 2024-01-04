
#include <assert.h>

#include "../customizedReader/binaryUtils.h"
#include "lcs.h"
#include "readerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *print_lcs_trace_format(lcs_trace_header_t *header) {
  static __thread char buf[512];
  snprintf(buf, 512,
           "lcs trace format: %s, obj_id_field %d, time_field %d, "
           "obj_size_field %d, next_access_vtime_field %d\n",
           header->format, header->obj_id_field, header->time_field,
           header->obj_size_field, header->next_access_vtime_field);

  return buf;
}

bool verify_LCS_trace_header(lcs_trace_header_t *header) {
  if (header->start_magic != LCS_TRACE_START_MAGIC) {
    ERROR("invalid trace file, start magic is wrong 0x%lx\n",
          (unsigned long)header->start_magic);
    return false;
  }

  if (header->end_magic != LCS_TRACE_END_MAGIC) {
    ERROR("invalid trace file, end magic is wrong 0x%lx\n",
          (unsigned long)header->end_magic);
    return false;
  }

  if (header->n_fields != strlen(header->format) - 1) {
    ERROR("invalid trace file, n_fields %d != format length %ld (format %s)\n",
          header->n_fields, strlen(header->format) - 1, header->format);
    return false;
  }

  if (header->obj_id_field > header->n_fields) {
    ERROR("invalid trace file, obj_id_field %d > n_fields %d\n",
          header->obj_id_field, header->n_fields);
    return false;
  }

  int item_size = 0;
  /* the first is little-endian */
  for (int i = 1; i < header->n_fields + 1; i++) {
    item_size += format_to_size(header->format[i]);
  }
  if (item_size != header->item_size) {
    ERROR(
        "invalid trace file, item_size %d != calculated item_size %d, format "
        "string %s\n",
        header->item_size, item_size, header->format);
    return false;
  }

  return true;
}

int LCSReader_setup(reader_t *reader) {
  // read the header
  assert(sizeof(lcs_trace_header_t) == 1024);
  reader->item_size = sizeof(lcs_trace_header_t);
  char *data = read_bytes(reader);
  lcs_trace_header_t *header = (lcs_trace_header_t *)data;

  if (!verify_LCS_trace_header(header)) {
    ERROR("invalid LCS trace\n");
  }

  reader->init_params.time_field = header->time_field;
  reader->init_params.obj_id_field = header->obj_id_field;
  reader->init_params.obj_size_field = header->obj_size_field;
  reader->init_params.next_access_vtime_field = header->next_access_vtime_field;
  reader->init_params.op_field = header->op_field;
  reader->init_params.ttl_field = header->ttl_field;
  reader->init_params.binary_fmt_str = strdup(header->format);
  reader->init_params.trace_start_offset = sizeof(lcs_trace_header_t);
  reader->trace_start_offset = sizeof(lcs_trace_header_t);

  binaryReader_setup(reader);

  if (reader->item_size != (size_t)header->item_size) {
    ERROR(
        "LCS trace corruption, item size in header %d is different from "
        "calculated using format string %d\n",
        header->item_size, (int)reader->item_size);
  }

  return 0;
}

#ifdef __cplusplus
}
#endif

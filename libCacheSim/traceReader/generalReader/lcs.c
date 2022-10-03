
#include "lcs.h"

#include <assert.h>

#include "../customizedReader/binaryUtils.h"
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

lcs_trace_header_t parse_header(const void *header_str) {
  lcs_trace_header_t header;
  assert(sizeof(lcs_trace_header_t) == 512);

  memcpy(&header, header_str, sizeof(lcs_trace_header_t));
  return header;
}

int LCSReader_setup(reader_t *reader) {
  // read the header
  reader->item_size = sizeof(lcs_trace_header_t);
  char *data = read_bytes(reader);
  lcs_trace_header_t *header = (lcs_trace_header_t *)data;

  if (header->start_magic != LCS_TRACE_START_MAGIC)
    ERROR("invalid trace file, start magic is wrong 0x%lx\n", header->start_magic);

  if (header->end_magic != LCS_TRACE_END_MAGIC)
    ERROR("invalid trace file, end magic is wrong 0x%lx\n", header->end_magic);

  reader->init_params.time_field = header->time_field;
  reader->init_params.obj_id_field = header->obj_id_field;
  reader->init_params.obj_size_field = header->obj_size_field;
  reader->init_params.next_access_vtime_field = header->next_access_vtime_field;
  reader->init_params.op_field = header->op_field;
  reader->init_params.ttl_field = header->ttl_field;
  reader->init_params.binary_fmt_str = strdup(header->format);

  binaryReader_setup(reader);

  reader->trace_start_offset = sizeof(lcs_trace_header_t);
  return 0;
}

#ifdef __cplusplus
}
#endif

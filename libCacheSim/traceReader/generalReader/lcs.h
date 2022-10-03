#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// 512 bytes
typedef struct lcs_trace_header {
  uint64_t magic_start;  // 0x123456789abcdef0
  // format string, following Python struct module, starts
  // with endien and followed by the format, currently only
  // support little-endian, with c: int8_t, b: int8_t, B:
  // uint8_t, h: int16_t, H: uint16_t, i: int32_t, I:
  // uint32_t, q: int64_t, Q: uint64_t, f: float, d: double,
  // https://docs.python.org/3/library/struct.html#format-strings
  // the current version supports at most 240 fields
  char format[240];
  // of the 240 fields, which one is the object id field
  // field index starts from 1
  unsigned char obj_id_field;
  unsigned char obj_size_field;
  unsigned char time_field;
  // when it is last requested, vtime is reference count
  unsigned char last_access_vtime_field;
  // when it is next requested, vtime is reference count
  unsigned char next_access_vtime_field;
  // of the 240 fields, which one is the operation field
  unsigned char op_field;
  unsigned char ttl_field;
  unsigned char tenant_field;
  unsigned char content_type_field;
  unsigned char reserved[231];
  // the number of fields
  uint32_t n_fields;
  // the size of each request
  uint32_t item_size;
  uint64_t version;
  uint64_t magic_end;  // 0x123456789abcdef0
} lcs_trace_header_t;


const char *print_lcs_trace_format(lcs_trace_header_t *header); 

void parse_lcs_trace_header(lcs_trace_header_t *header, const char *header_str);

static inline int lcsReader_setup(reader_t *reader, const char *header_str) {
  lcs_trace_header_t header;
  parse_lcs_trace_header(&header, header_str);
  
  // reader->trace_type = LCS_TRACE;
  // reader->trace_format = BINARY_TRACE_FORMAT;
  // reader->item_size = 22; /* IqIhhh */
  // reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int lcs_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = false;
    return 1;
  }

  req->real_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->tenant_id = *(uint16_t *)(record + 16);
  req->bucket_id = *(uint16_t *)(record + 18);
  req->content_type = *(uint16_t *)(record + 20);

  if (req->obj_size == 0 && reader->ignore_size_zero_req)
    return lcs_read_one_req(reader, req);

  return 0;
}


#ifdef __cplusplus
}
#endif
#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
    extern "C" {
#endif

#define LCS_TRACE_START_MAGIC 0x123456789abcdef0
#define LCS_TRACE_END_MAGIC 0x123456789abcdef0

// 1024 bytes
typedef struct lcs_trace_header {
  uint64_t start_magic;
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

  /* trace stat */
  struct {
    int64_t n_req; // number of requests
    int64_t n_obj; // number of objects
    int64_t n_req_byte; // number of bytes requested
    int64_t n_obj_byte; // number of bytes of objects

    int64_t unused[60];
  };
  // the number of fields
  uint32_t n_fields;
  // the size of each request
  uint32_t item_size;
  uint64_t version;
  uint64_t end_magic;  // 0x123456789abcdef0
} lcs_trace_header_t;

int LCSReader_setup(reader_t *reader);

bool verify_LCS_trace_header(lcs_trace_header_t *header); 

#ifdef __cplusplus
}
#endif


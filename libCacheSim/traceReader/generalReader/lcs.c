/**
 * libCacheSim binary trace format
 *
 * the trace has a header of 512 bytes, which is used to store the metadata
 * struct lcs_trace_header {
 *
 *   uint64_t magic_start; // 0x123456789abcdef0
 *
 *  // format string, following Python struct module, starts with endien
 *  // and followed by the format, currently only support little-endian, with
 *  // c: int8_t, b: int8_t, B: uint8_t,
 *  // h: int16_t, H: uint16_t,
 *  // i: int32_t, I: uint32_t,
 *  // q: int64_t, Q: uint64_t,
 *  // f: float, d: double,
 *  // https://docs.python.org/3/library/struct.html#format-strings
 *  // the current version supports at most 240 fields
 *  char format[240];
 *
 *  // of the 240 fields, which one is the object id field
 *  // field index starts from 1
 *  unsigned char obj_id_field;
 *
 *  unsigned char obj_size_field;
 *
 *  unsigned char time_field;
 *
 *  // when it is last requested, vtime is reference count
 *  unsigned char last_access_vtime_field;
 *
 *  // when it is next requested, vtime is reference count
 *  unsigned char next_access_vtime_field;
 *
 *  // of the 240 fields, which one is the operation field
 *  unsigned char op_field;
 *
 *  unsigned char ttl_field;
 *
 *  unsigned char tenant_field;
 *
 *  unsigned char content_type_field;
 *
 *
 *  unsigned char reserved[119];
 *
 *
 *  // the number of fields
 *  uint32_t n_fields;
 *
 *  // the size of each request
 *  uint32_t item_size;
 *
 *  uint64_t version;
 *
 *  uint64_t magic_end; // 0x123456789abcdef0
 * };
 */


#include "lcs.h"

#ifdef __cplusplus
extern "C" {
#endif


lcs_trace_header_t parse_header(const void* header_str) {
    lcs_trace_header_t header;
    assert(sizeof(lcs_trace_header_t) == 512);

    memcpy(&header, header_str, sizeof(lcs_trace_header_t));
    return header;
}


#ifdef __cplusplus
}
#endif


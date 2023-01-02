#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t obj_size;
 *   int16_t tenant_id;
 *   int16_t bucket_id;
 *   int16_t content;
 *   int64_t next_access_vtime;
 * }__attribute__((packed));
 *
 */

#include "../../../include/libCacheSim/reader.h"

static inline int oracleAkamai_setup(reader_t *reader) {
  reader->trace_type = ORACLE_AKAMAI_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 30;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int oracleAkamai_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->tenant_id = *(int16_t *)(record + 16) - 1;
  req->bucket_id = *(int16_t *)(record + 18) - 1;
  req->content_type = *(int16_t *)(record + 20) - 1;
  req->next_access_vtime = *(int64_t *)(record + 22);

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return oracleAkamai_read_one_req(reader, req);
  return 0;
}

#ifdef __cplusplus
}
#endif

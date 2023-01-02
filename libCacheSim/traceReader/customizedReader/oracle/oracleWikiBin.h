#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*
 * wiki 2016 upload format
 * struct {
 *   uint64_t obj_id;
 *   uint32_t size;
 *   uint16_t content_type;
 *   int64_t next_access_vtime;
 * }
 *
 *
 * wiki 2019 upload format
 * struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t size;
 *   uint16_t content_type;
 *   int64_t next_access_vtime;
 * }
 *
 *
 * wiki 2019 txt format
 * struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t size;
 *   int64_t next_access_vtime;
 * }
 *
 *
 */

#include "../../../include/libCacheSim/reader.h"

static inline int oracleWiki2016uReader_setup(reader_t *reader) {
  reader->trace_type = ORACLE_WIKI16u_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 22;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int oracleWiki2016u_read_one_req(reader_t *reader,
                                               request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = 0;
  req->obj_id = *(uint64_t *)(record);
  req->obj_size = *(uint32_t *)(record + 8);
  req->content_type = *(uint16_t *)(record + 12);
  req->next_access_vtime = *(uint64_t *)(record + 14);

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return oracleWiki2016u_read_one_req(reader, req);

  return 0;
}

static inline int oracleWiki2019uReader_setup(reader_t *reader) {
  reader->trace_type = ORACLE_WIKI19u_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 26;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int oracleWiki2019u_read_one_req(reader_t *reader,
                                               request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->content_type = *(uint16_t *)(record + 16);
  req->next_access_vtime = *(uint64_t *)(record + 18);

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return oracleWiki2019u_read_one_req(reader, req);

  return 0;
}


#ifdef __cplusplus
}
#endif

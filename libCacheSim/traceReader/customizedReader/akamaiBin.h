#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*
 * format
 * struct {
 *  uint32_t real_time;
 *  uint64_t obj_id;
 *  uint32_t size;
 *  uint16_t tenant_id;
 *  uint16_t bucket_id;
 *  uint16_t content_type;
 * }
 *
 */

#include <math.h>

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

#define MAX_OBJ_SIZE (1048576L * 4000L)

static inline int akamaiReader_setup(reader_t *reader) {
  reader->trace_type = AKAMAI_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 22; /* IqIhhh */
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int akamai_read_one_req(reader_t *reader, request_t *req) {
  char *record = NULL;

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  //  if (reader->cnt >= 1) {
  //    reader->cnt -= 1;
  //    if (reader->leftover_size > MAX_OBJ_SIZE)
  //      req->obj_size = MAX_OBJ_SIZE;
  //    else
  //      req->obj_size = reader->leftover_size;
  //    reader->leftover_size -= req->obj_size;
  //    req->obj_id += 1;
  //    return 0;
  //  }

  req->real_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->tenant_id = *(uint16_t *)(record + 16);
  req->bucket_id = *(uint16_t *)(record + 18);
  req->content_type = *(uint16_t *)(record + 20);

  //  if (req->obj_size > MAX_OBJ_SIZE) {
  //    reader->cnt = ceil((double)req->obj_size/MAX_OBJ_SIZE) - 1;
  //    reader->leftover_size = req->obj_size - MAX_OBJ_SIZE;
  //    req->obj_size = MAX_OBJ_SIZE;
  //  } else {
  //    reader->cnt = 0;
  //  }

  if (req->obj_size == 0 && reader->ignore_size_zero_req)
    return akamai_read_one_req(reader, req);

  return 0;
}

#ifdef __cplusplus
}
#endif

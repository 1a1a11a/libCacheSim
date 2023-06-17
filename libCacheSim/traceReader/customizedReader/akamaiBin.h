#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*
 * format
 * struct req {
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

/**
 * @brief setup the reader for akamai trace
 *
 * @param reader
 * @return int
 */
static inline int akamaiReader_setup(reader_t *reader) {
  /* set the trace format */
  reader->trace_type = AKAMAI_TRACE;

  /* most added traces are binary, the other format is TXT_TRACE_FORMAT  */
  reader->trace_format = BINARY_TRACE_FORMAT;

  /* the size of one request in the trace, this is sizeof(struct req) */
  reader->item_size = 22; /* IqIhhh */

  /* calculate the number of requests in the trace */
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);

  /* the object id is numerical so do not convert it again, this is useful for
   * TXT_TRACE_FORMAT */
  reader->obj_id_is_num = true;

  return 0;
}

/**
 * @brief read one request from the trace
 *
 * @param reader
 * @param req
 * @return int
 */
static inline int akamai_read_one_req(reader_t *reader, request_t *req) {
  /* read reader->item_size bytes from the trace */
  char *record = read_bytes(reader);

  if (record == NULL) {
    /* end of the trace or error */
    req->valid = false;
    return 1;
  }

  /* read the trace */
  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->tenant_id = *(uint16_t *)(record + 16);
  req->bucket_id = *(uint16_t *)(record + 18);
  req->content_type = *(uint16_t *)(record + 20);

  /* if we read a request of size 0 and the trace is reading forward,
     read the next request */
  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return akamai_read_one_req(reader, req);

  return 0;
}

#ifdef __cplusplus
}
#endif

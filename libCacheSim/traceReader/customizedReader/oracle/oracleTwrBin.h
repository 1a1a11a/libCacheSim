/*
 * format
 * struct {
 *     uint32_t real_time;
 *     uint64_t obj_id;
 *     uint32_t obj_size;
 *     uint32_t ttl;
 *     int64_t next_access_vtime;
 * }
 *
 * struct reqEntryReuseRealSystemTwrNS {
 *   // IQHIHHIQ
 *   uint32_t real_time_;
 *   uint64_t obj_id_;
 *   uint16_t key_size_;
 *   uint32_t val_size_;
 *   uint16_t op_;
 *   uint16_t ns;
 *   int32_t ttl_;
 *   int64_t next_access_vtime_;
 * }
 *
 */

#pragma once

#include "../../../include/libCacheSim/reader.h"
#include "../binaryUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int oracleSimTwrBin_setup(reader_t *reader) {
  reader->trace_type = ORACLE_SIM_TWR_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 28;
  reader->obj_id_is_num = true;

  /* all oracle twr traces are oracle twrNS traces */
  ERROR("this struct is not correct, need update\n");
  abort();
  return 0;
}

static inline int oracleSimTwrBin_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader, reader->item_size);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint16_t *)(record + 12);
  req->ttl = *(uint16_t *)(record + 16);
  req->next_access_vtime = *(int64_t *)(record + 20);
  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    req->next_access_vtime = MAX_REUSE_DISTANCE;
  }

  if (req->val_size == 0 && reader->ignore_size_zero_req && (req->op == OP_GET || req->op == OP_GETS) &&
      reader->read_direction == READ_FORWARD)
    return oracleSimTwrBin_read_one_req(reader, req);

  return 0;
}

static inline int oracleSysTwrBin_setup(reader_t *reader) {
  reader->trace_type = ORACLE_SYS_TWR_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 34;

  /* all oracle twr traces are oracle twrNS traces */
  ERROR("this struct is not correct, need update\n");
  abort();
  return 0;
}

static inline int oracleSysTwrBin_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader, reader->item_size);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->key_size = *(uint16_t *)(record + 12);
  req->val_size = *(uint32_t *)(record + 14);
  req->obj_size = req->key_size + req->val_size;
  req->op = (req_op_e)(*(uint16_t *)(record + 18));
  req->ns = *(uint16_t *)(record + 20);
  req->ttl = *(int32_t *)(record + 22);
  req->next_access_vtime = *(int64_t *)(record + 26);
  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    req->next_access_vtime = MAX_REUSE_DISTANCE;
  }

  if (req->val_size == 0 && reader->ignore_size_zero_req && (req->op == OP_GET || req->op == OP_GETS) &&
      reader->read_direction == READ_FORWARD)
    return oracleSimTwrBin_read_one_req(reader, req);

  return 0;
}

#ifdef __cplusplus
}
#endif

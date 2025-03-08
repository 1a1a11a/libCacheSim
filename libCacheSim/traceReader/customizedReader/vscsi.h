#pragma once
/**
 * vscsi reader for vscsi trace
 *
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TEST 2

typedef struct {
  uint64_t ts;
  uint32_t len;
  uint64_t lbn;
  uint16_t cmd;
} trace_item_t;

typedef struct {
  uint32_t sn;
  uint32_t len;
  uint32_t nSG;
  uint16_t cmd;
  uint16_t ver;
  uint64_t lbn;
  uint64_t ts;
} trace_v1_record_t;

typedef struct {
  uint16_t cmd;
  uint16_t ver;
  uint32_t sn;
  uint32_t len;
  uint32_t nSG;
  uint64_t lbn;
  uint64_t ts;
  uint64_t rt;
} trace_v2_record_t;

typedef enum { VSCSI1 = 1, VSCSI2 = 2, UNKNOWN } vscsi_version_e;

typedef struct {
  vscsi_version_e vscsi_ver;
} vscsi_params_t;

static inline bool test_version(const vscsi_version_e *test_buf,
                                vscsi_version_e version) {
  for (int i = 0; i < MAX_TEST; i++) {
    if (version != test_buf[i]) return (false);
  }
  return (true);
}

static inline vscsi_version_e to_vscsi_version(uint16_t ver) {
  return ((vscsi_version_e)(ver >> 8));
}

static inline vscsi_version_e test_vscsi_version(void *trace) {
  vscsi_version_e test_buf[MAX_TEST] = {};

  int i;
  for (i = 0; i < MAX_TEST; i++) {
    test_buf[i] = to_vscsi_version((((trace_v2_record_t *)trace)[i]).ver);
  }
  if (test_version(test_buf, VSCSI2))
    return (VSCSI2);
  else {
    for (i = 0; i < MAX_TEST; i++) {
      test_buf[i] = to_vscsi_version((((trace_v1_record_t *)trace)[i]).ver);
    }

    if (test_version(test_buf, VSCSI1)) return (VSCSI1);
  }
  return (UNKNOWN);
}

static inline size_t record_size(vscsi_version_e version) {
  if (version == VSCSI1)
    return (sizeof(trace_v1_record_t));
  else if (version == VSCSI2)
    return (sizeof(trace_v2_record_t));
  else
    return (-1);
}

static inline int vscsiReader_setup(reader_t *const reader) {
  vscsi_params_t *params = malloc(sizeof(vscsi_params_t));
  reader->reader_params = params;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->obj_id_is_num = true;

  /* Version 2 records are the bigger of the two */
  if ((unsigned long long)reader->file_size <
      (unsigned long long)sizeof(trace_v2_record_t) * MAX_TEST) {
    ERROR("File too small, unable to read header.\n");
    return -1;
  }

  vscsi_version_e ver = test_vscsi_version(reader->mapped_file);
  switch (ver) {
    case VSCSI1:
    case VSCSI2:
      reader->item_size = record_size(ver);
      params->vscsi_ver = ver;
      break;

    case UNKNOWN:
    default:
      ERROR("Trace format unrecognized.\n");
      return -1;
  }
  return 0;
}

static inline int vscsi_read_ver1(reader_t *reader, request_t *req) {
  trace_v1_record_t *record =
      (trace_v1_record_t *)(reader->mapped_file + reader->mmap_offset);
  // trace uses microsec change to sec
  req->clock_time = record->ts / 1000000;
  req->obj_size = record->len;
  uint16_t cmd = record->cmd;
  if (cmd == 40 || cmd == 8 || cmd == 136 || cmd == 45 || cmd == 168) {
      req->op = OP_READ;
  } else if (cmd == 42 || cmd == 63 || cmd == 138 || cmd == 142 ||
             cmd == 154 || cmd == 156 || cmd == 170 || cmd == 174) {
      req->op = OP_WRITE;
  } else {
      req->op = OP_INVALID;
  }
  req->obj_id = record->lbn;
  (reader->mmap_offset) += reader->item_size;
  return 0;
}

static inline int vscsi_read_ver2(reader_t *reader, request_t *req) {
  trace_v2_record_t *record =
      (trace_v2_record_t *)(reader->mapped_file + reader->mmap_offset);
  req->clock_time = record->ts / 1000000;
  req->obj_size = record->len;
  uint16_t cmd = record->cmd;
  if (cmd == 40 || cmd == 8 || cmd == 136 || cmd == 45 || cmd == 168) {
      req->op = OP_READ;
  } else if (cmd == 42 || cmd == 63 || cmd == 138 || cmd == 142 ||
             cmd == 154 || cmd == 156 || cmd == 170 || cmd == 174) {
      req->op = OP_WRITE;
  } else {
      req->op = OP_INVALID;
  }
  req->obj_id = record->lbn;
  (reader->mmap_offset) += reader->item_size;
  return 0;
}

static inline int vscsi_read_one_req(reader_t *reader, request_t *req) {
  if (reader->mmap_offset >= reader->n_total_req * reader->item_size) {
    req->valid = false;
    return 1;
  }

  vscsi_params_t *params = (vscsi_params_t *)reader->reader_params;

  int (*fptr)(reader_t *, request_t *) = NULL;
  switch (params->vscsi_ver) {
    case VSCSI1:
      fptr = vscsi_read_ver1;
      break;

    case VSCSI2:
      fptr = vscsi_read_ver2;
      break;
    case UNKNOWN:
      ERROR("unknown vscsi version encountered\n");
      break;
  }
  return fptr(reader, req);
}

#ifdef __cplusplus
}
#endif

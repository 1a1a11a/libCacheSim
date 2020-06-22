//
//  vscsiReader.c
//  libCacheSim
//
//  Created by CloudPhysics
//  Modified by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "include/vscsi.h"

#ifdef __cplusplus
extern "C"
{
#endif

static inline bool test_version(vscsi_version_e *test_buf,
                                vscsi_version_e version) {
  int i;
  for (i = 0; i < MAX_TEST; i++) {
    if (version != test_buf[i])
      return (false);
  }
  return (true);
}

static inline vscsi_version_e to_vscsi_version(uint16_t ver) {
  return ((vscsi_version_e) (ver >> 8));
}

static inline void v1_extract_trace_item(void *record,
                                         trace_item_t *trace_item) {
  trace_v1_record_t *rec = (trace_v1_record_t *) record;
  trace_item->ts = rec->ts;
  trace_item->cmd = rec->cmd;
  trace_item->len = rec->len;
  trace_item->lbn = rec->lbn;
}

static inline void v2_extract_trace_item(void *record,
                                         trace_item_t *trace_item) {
  trace_v2_record_t *rec = (trace_v2_record_t *) record;
  trace_item->ts = rec->ts;
  trace_item->cmd = rec->cmd;
  trace_item->len = rec->len;
  trace_item->lbn = rec->lbn;
}

static inline size_t record_size(vscsi_version_e version) {
  if (version == VSCSI1)
    return (sizeof(trace_v1_record_t));
  else if (version == VSCSI2)
    return (sizeof(trace_v2_record_t));
  else
    return (-1);
}

int vscsi_setup(reader_t *const reader) {
  vscsi_params_t *params = g_new0(vscsi_params_t, 1);
  reader->reader_params = params;

  /* Version 2 records are the bigger of the two */
  if ((unsigned long long) reader->file_size <
      (unsigned long long) sizeof(trace_v2_record_t) * MAX_TEST) {
    ERROR("File too small, unable to read header.\n");
    return -1;
  }

  reader->obj_id_type = OBJ_ID_NUM;

  vscsi_version_e ver = test_vscsi_version(reader->mapped_file);
  switch (ver) {
    case VSCSI1:
    case VSCSI2:reader->item_size = record_size(ver);
      params->vscsi_ver = ver;
      reader->n_total_req = reader->file_size / (reader->item_size);
      break;

    case UNKNOWN:
    default:ERROR("Trace format unrecognized.\n");
      return -1;
  }
  return 0;
}

vscsi_version_e test_vscsi_version(void *trace) {
  vscsi_version_e test_buf[MAX_TEST] = {};

  int i;
  for (i = 0; i < MAX_TEST; i++) {
    test_buf[i] =
        (vscsi_version_e) ((((trace_v2_record_t *) trace)[i]).ver >> 8);
  }
  if (test_version(test_buf, VSCSI2))
    return (VSCSI2);
  else {
    for (i = 0; i < MAX_TEST; i++) {
      test_buf[i] =
          (vscsi_version_e) ((((trace_v1_record_t *) trace)[i]).ver >> 8);
    }

    if (test_version(test_buf, VSCSI1))
      return (VSCSI1);
  }
  return (UNKNOWN);
}

static inline int vscsi_read_ver1(reader_t *reader, request_t *c) {
  trace_v1_record_t *record = (trace_v1_record_t *) (reader->mapped_file
      + reader->mmap_offset);
  // trace uses microsec change to sec
  c->real_time = record->ts / 1000000;
  c->obj_size = record->len;
  /* need to parse this */
  c->op = record->cmd;
  c->obj_id_int = record->lbn;
  (reader->mmap_offset) += reader->item_size;
  return 0;
}

static inline int vscsi_read_ver2(reader_t *reader, request_t *c) {
  trace_v2_record_t *record = (trace_v2_record_t *) (reader->mapped_file
      + reader->mmap_offset);
  c->real_time = record->ts / 1000000;
  c->obj_size = record->len;
  c->op = record->cmd;
  c->obj_id_int = record->lbn;
  (reader->mmap_offset) += reader->item_size;
  return 0;
}

int vscsi_read_one_req(reader_t *reader, request_t *c) {
  if (reader->mmap_offset >= reader->n_total_req
      * reader->item_size) {
    c->valid = false;
    return 1;
  }

  vscsi_params_t *params = (vscsi_params_t *) reader->reader_params;

  int (*fptr)(reader_t *, request_t *) = NULL;
  switch (params->vscsi_ver) {
    case VSCSI1:fptr = vscsi_read_ver1;
      break;

    case VSCSI2:fptr = vscsi_read_ver2;
      break;
    case UNKNOWN: ERROR("unknown vscsi version encountered\n");
      break;
  }
  return fptr(reader, c);
}

#ifdef __cplusplus
}
#endif

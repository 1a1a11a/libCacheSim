
#include "lcs.h"

#include <assert.h>

#include "../customizedReader/binaryUtils.h"
#include "readerInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

static bool verify(lcs_trace_header_t *header) {
  /* check whether the trace is valid */
  if (header->start_magic != LCS_TRACE_START_MAGIC) {
    ERROR("invalid trace file, start magic is wrong 0x%lx\n", (unsigned long)header->start_magic);
    return false;
  }

  if (header->end_magic != LCS_TRACE_END_MAGIC) {
    ERROR("invalid trace file, end magic is wrong 0x%lx\n", (unsigned long)header->end_magic);
    return false;
  }

  if (header->version > MAX_LCS_VERSION) {
    ERROR("invalid trace file, lcs version %ld is not supported\n", (unsigned long)header->version);
    return false;
  }

  lcs_trace_stat_t *stat = &(header->stat);
  if (stat->n_req < 0 || stat->n_obj < 0) {
    ERROR("invalid trace file, n_req %ld, n_obj %ld\n", (unsigned long)stat->n_req, (unsigned long)stat->n_obj);
    return false;
  }

  return true;
}

int lcsReader_setup(reader_t *reader) {
  // read the header
  assert(sizeof(lcs_trace_header_t) == 1024);
  assert(sizeof(lcs_trace_stat_t) == 512);
  assert(sizeof(lcs_req_v1_t) == 24);
  assert(sizeof(lcs_req_v2_t) == 28);

  char *data = read_bytes(reader, reader->item_size);
  lcs_trace_header_t *header = (lcs_trace_header_t *)data;

  if (!verify(header)) {
    exit(1);
  }

  reader->lcs_ver = header->version;
  reader->trace_type = LCS_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->trace_start_offset = sizeof(lcs_trace_header_t);
  reader->obj_id_is_num = true;

  if (reader->lcs_ver == 1) {
    reader->item_size = sizeof(lcs_req_v1_t);
  } else if (reader->lcs_ver == 2) {
    reader->item_size = sizeof(lcs_req_v2_t);
  } else {
    ERROR("invalid lcs version %ld\n", (unsigned long)reader->lcs_ver);
    exit(1);
  }

  lcs_print_trace_stat(reader);

  return 0;
}

// read one request from trace file
// return 0 if success, 1 if error
int lcs_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader, reader->item_size);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  if (reader->lcs_ver == 1) {
    lcs_req_v1_t *req_v1 = (lcs_req_v1_t *)record;
    req->clock_time = req_v1->clock_time;
    req->obj_id = req_v1->obj_id;
    req->obj_size = req_v1->obj_size;
    req->next_access_vtime = req_v1->next_access_vtime;
  } else if (reader->lcs_ver == 2) {
    lcs_req_v2_t *req_v2 = (lcs_req_v2_t *)record;
    req->clock_time = req_v2->clock_time;
    req->obj_id = req_v2->obj_id;
    req->obj_size = req_v2->obj_size;
    req->next_access_vtime = req_v2->next_access_vtime;
    req->tenant_id = req_v2->tenant;
    req->op = req_v2->op;
  } else {
    ERROR("invalid lcs version %ld\n", (unsigned long)reader->lcs_ver);
    return 1;
  }

  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    req->next_access_vtime = MAX_REUSE_DISTANCE;
  }

  if (req->obj_size == 0 && reader->ignore_size_zero_req && reader->read_direction == READ_FORWARD) {
    return lcs_read_one_req(reader, req);
  }
  return 0;
}

void lcs_print_trace_stat(reader_t *reader) {
  // we need to reset the reader so clone a new one
  reader_t *cloned_reader = clone_reader(reader);
  reset_reader(cloned_reader);
  char *data = read_bytes(cloned_reader, sizeof(lcs_trace_header_t));
  lcs_trace_header_t *header = (lcs_trace_header_t *)data;
  lcs_trace_stat_t *stat = &(header->stat);

  printf("trace stat: n_ttl %d, smallest_ttl %d, largest_ttl %d, time_unit %d, trace_type %d\n", stat->n_ttl,
         stat->smallest_ttl, stat->largest_ttl, stat->time_unit, stat->trace_type);

  close_reader(cloned_reader);
}

#ifdef __cplusplus
}
#endif

//
//  readerRequest.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "customizedReader/lcs.h"
#include "customizedReader/oracle/oracleGeneralBin.h"
#include "customizedReader/oracle/oracleTwrBin.h"
#include "customizedReader/oracle/oracleTwrNSBin.h"
#include "customizedReader/twrBin.h"
#include "customizedReader/twrNSBin.h"
#include "customizedReader/valpinBin.h"
#include "customizedReader/vscsi.h"
#include "generalReader/libcsv.h"
#include "generalReader/synthetic.h"
#include "libCacheSim/macro.h"
#include "readerInternal.h"

/**
 * @brief read the next request from the source
 *
 * @param reader
 * @param req
 * @return 0 if success, 1 if end of file
 */
int read_one_req(reader_t *const reader, request_t *const req) {
  if (reader->mmap_offset >= reader->file_size &&
      (reader->trace_type != SYNTHETIC_TRACE || !synthetic_has_next(reader))) {
    DEBUG("read_one_req: end of file, current mmap_offset %zu, file size %zu\n",
          reader->mmap_offset, reader->file_size);
    req->valid = false;
    return 1;
  }

  if (reader->cap_at_n_req > 1 && reader->n_read_req >= reader->cap_at_n_req) {
    DEBUG("read_one_req: processed %ld requests capped by the user\n",
          (long)reader->n_read_req);
    req->valid = false;
    return 1;
  }

  int status = 0;
  size_t offset_before_read = reader->mmap_offset;
  if (reader->n_req_left > 0) {
    reader->n_req_left -= 1;
    req->clock_time = reader->last_req_clock_time;

  } else {
    reader->n_read_req += 1;
    req->hv = 0;
    req->ttl = 0;
    req->obj_size = 1;
    req->obj_cost = 1;
    req->valid = true;

    switch (reader->trace_type) {
      case CSV_TRACE:
        offset_before_read = ftell(reader->file);
        status = csv_read_one_req(reader, req);
        break;
      case PLAIN_TXT_TRACE:;
        offset_before_read = ftell(reader->file);
        status = txt_read_one_req(reader, req);
        break;
      case BIN_TRACE:
        status = binary_read_one_req(reader, req);
        break;
      case VSCSI_TRACE:
        status = vscsi_read_one_req(reader, req);
        break;
      case TWR_TRACE:
        status = twr_read_one_req(reader, req);
        break;
      case TWRNS_TRACE:
        status = twrNS_read_one_req(reader, req);
        break;
      case ORACLE_GENERAL_TRACE:
        status = oracleGeneralBin_read_one_req(reader, req);
        break;
      case ORACLE_SIM_TWR_TRACE:
        status = oracleSimTwrBin_read_one_req(reader, req);
        break;
      case ORACLE_SYS_TWRNS_TRACE:
        status = oracleSysTwrNSBin_read_one_req(reader, req);
        break;
      case ORACLE_SIM_TWRNS_TRACE:
        status = oracleSimTwrNSBin_read_one_req(reader, req);
        break;
      case LCS_TRACE:
        status = lcs_read_one_req(reader, req);
        break;
      case VALPIN_TRACE:
        status = valpin_read_one_req(reader, req);
        break;
      case SYNTHETIC_TRACE:
        status = synthetic_read_one_req(reader, req);
        break;
      default:
        ERROR(
            "cannot recognize reader trace_type, given reader trace_type: "
            "%c\n",
            reader->trace_type);
        abort();
    }
  }

  if (reader->sampler != NULL) {
    /* we use this complex solution rather than simply recursive calls
       because recursive calls can lead to stack overflow */
    sampler_t *sampler = reader->sampler;
    reader->sampler = NULL;
    while (!sampler->sample(sampler, req)) {
      VERBOSE("skip one req: time %lu, obj_id %lu, size %lu at offset %zu\n",
              req->clock_time, req->obj_id, req->obj_size, offset_before_read);
      if (reader->read_direction == READ_FORWARD) {
        status = read_one_req(reader, req);
      } else {
        status = read_one_req_above(reader, req);
      }
      if (status != 0) {
        reader->sampler = sampler;
        return status;
      }
    }
    reader->sampler = sampler;
  }

  if (reader->ignore_obj_size) {
    req->obj_size = 1;
  }

  if (reader->init_params.ignore_ttl) {
    req->ttl = 0;
  }

  if (reader->init_params.obj_cost_field <= 0) {
    req->obj_cost = 1;
  }

  VERBOSE(
      "read one req: time %lu, obj_id %lu, size %lu, cost %lu at offset %zu\n",
      req->clock_time, req->obj_id, req->obj_size, req->obj_cost,
      offset_before_read);

  return status;
}

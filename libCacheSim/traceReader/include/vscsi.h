//
//  vscsiReader.h
//  libCacheSim
//
//  Created by CloudPhysics
//  Modified by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifndef VSCSI_READER_H
#define VSCSI_READER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/libCacheSim.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

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

typedef enum {
  VSCSI1 = 1,
  VSCSI2 = 2,
  UNKNOWN
} vscsi_version_e;

typedef struct {
  vscsi_version_e vscsi_ver;
} vscsi_params_t;

int vscsiReader_setup(reader_t *const reader);

int vscsi_read_one_req(reader_t *reader, request_t *c);

vscsi_version_e test_vscsi_version(void *trace);

#ifdef __cplusplus
}
#endif

#endif  /* VSCSI_READER_H */

//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef libCacheSim_ENUM_H
#define libCacheSim_ENUM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  RETURN_OK,
  RETURN_ERROR,

  RETURN_INVALID
} rstatus_e;

typedef enum {
  BINARY_TRACE_FORMAT,
  TXT_TRACE_FORMAT,

  INVALID_TRACE_FORMAT
} trace_format_e;

// trace type
typedef enum {
  CSV_TRACE,
  BIN_TRACE,
  PLAIN_TXT_TRACE,

  VSCSI_TRACE,

  TWR_TRACE,
  TWRNS_TRACE,
  CF1_TRACE, 
  AKAMAI_TRACE,
  WIKI16u_TRACE,
  WIKI19u_TRACE,
  WIKI19t_TRACE,
  STANDARD_IQI_TRACE,
  STANDARD_IQQ_TRACE,
  STANDARD_III_TRACE,
  STANDARD_IQIBH_TRACE,

  ORACLE_GENERAL_TRACE,
  ORACLE_GENERALOPNS_TRACE,
  ORACLE_SIM_TWR_TRACE,
  ORACLE_SYS_TWR_TRACE,
  ORACLE_SIM_TWRNS_TRACE,
  ORACLE_SYS_TWRNS_TRACE,
  ORACLE_CF1_TRACE, 

  ORACLE_AKAMAI_TRACE,
  ORACLE_WIKI16u_TRACE,
  ORACLE_WIKI19u_TRACE,
  ORACLE_WIKI19t_TRACE,

  UNKNOWN_TRACE,
} __attribute__((__packed__)) trace_type_e;

// obj_id type
typedef enum {
  OBJ_ID_NUM = 'l',
  OBJ_ID_STR = 'c',

  UNKNOWN_OBJ_ID,
} __attribute__((__packed__)) obj_id_type_e;

typedef enum {
  OP_NOP = 0,
  OP_GET = 1,
  OP_GETS = 2,
  OP_SET = 3,
  OP_ADD = 4,
  OP_CAS = 5,
  OP_REPLACE = 6,
  OP_APPEND = 7,
  OP_PREPEND = 8,
  OP_DELETE = 9,
  OP_INCR = 10,
  OP_DECR = 11,

  OP_READ,
  OP_WRITE,
  OP_UPDATE,

  OP_INVALID,
} req_op_e;

#pragma GCC diagnostic ignored "-Wwrite-strings"
static char *OP_STR[OP_INVALID+2] = {
    "nop", "get", "gets", "set", "add", "cas", "replace", "append", "prepend",
    "delete", "incr", "decr", "read", "write", "update", "invalid"};

//enum op {
//  OP_GET = 0,
//  OP_GETS,
//  OP_SET,
//  OP_ADD,
//  OP_CAS,
//  OP_REPLACE,
//  OP_APPEND,
//  OP_PREPEND,
//  OP_DELETE,
//  OP_INCR,
//  OP_DECR,
//
//  OP_READ,
//  OP_WRITE,
//  OP_UPDATE,
//
//  OP_INVALID,
//} __attribute__((__packed__));
//typedef enum op req_op_e;
//
//#pragma GCC diagnostic ignored "-Wwrite-strings"
//static char *OP_STR[OP_INVALID+1] = {
//    "get", "gets", "set", "add", "cas", "replace", "append", "prepend",
//    "delete", "incr", "decr", "read", "write", "update", "invalid"};
//#pragma GCC diagnostic pop


typedef enum {
  cache_ck_hit = 0,
  cache_ck_miss = 1,
  cache_ck_expired = 2,

  cache_ck_invalid,
} __attribute__((__packed__)) cache_ck_res_e;

static char *CACHE_CK_STATUS_STR[cache_ck_invalid + 1] = {
    "hit", "miss", "expired", "invalid"
};

typedef enum {
  ERR,
  OK,
  MY_EOF
} rstatus;

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_ENUM_H

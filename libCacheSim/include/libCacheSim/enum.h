//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef libCacheSim_ENUM_H
#define libCacheSim_ENUM_H

#ifdef __cplusplus
extern "C" {
#endif

// trace type
typedef enum {
  CSV_TRACE,
  BIN_TRACE,
  PLAIN_TXT_TRACE,

  VSCSI_TRACE,
  TWR_TRACE,
  AKAMAI_TRACE,
  WIKI16u_TRACE,
  WIKI19u_TRACE,
  WIKI19t_TRACE,
  STANDARD_IQI_TRACE,
  STANDARD_III_TRACE,

  ORACLE_GENERAL_BIN,
  ORACLE_TWR_BIN,
  ORACLE_AKAMAI_BIN,
  ORACLE_WIKI16u_BIN,
  ORACLE_WIKI19u_BIN,
  ORACLE_WIKI19t_BIN,

  UNKNOWN_TRACE,
} __attribute__((__packed__)) trace_type_e;

// obj_id type
typedef enum {
  OBJ_ID_NUM = 'l',
  OBJ_ID_STR = 'c',

  UNKNOWN_OBJ_ID,
} __attribute__((__packed__)) obj_id_type_e;

enum op {
  OP_GET = 0,
  OP_GETS,
  OP_SET,
  OP_ADD,
  OP_CAS,
  OP_REPLACE,
  OP_APPEND,
  OP_PREPEND,
  OP_DELETE,
  OP_INCR,
  OP_DECR,

  OP_READ,
  OP_WRITE,
  OP_UPDATE,

  OP_INVALID,
} __attribute__((__packed__));
typedef enum op req_op_e;

#pragma GCC diagnostic ignored "-Wwrite-strings"
static char *OP_STR[OP_INVALID+1] = {
    "get", "gets", "set", "add", "cas", "replace", "append", "prepend",
    "delete", "incr", "decr", "read", "write", "update", "invalid"};
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

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_ENUM_H

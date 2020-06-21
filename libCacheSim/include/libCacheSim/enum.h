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
  CSV_TRACE = 'c',
  BIN_TRACE = 'b',
  PLAIN_TXT_TRACE = 'p',
  VSCSI_TRACE = 'v',
  TWR_TRACE = 't',

  UNKNOWN_TRACE = 'u',
} trace_type_e;

// obj_id type
typedef enum {
  OBJ_ID_NUM = 'l',
  OBJ_ID_STR = 'c',

  UNKNOWN_OBJ_ID,
} obj_id_type_e;

typedef enum {
  OP_GET = 1,
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
} req_op_e;

static char *OP_STR[OP_INVALID] = {
    "get",    "gets", "set",  "add",  "cas",   "replace", "append", "prepend",
    "delete", "incr", "decr", "read", "write", "update",  "invalid"};

/* this is inconsistent enum ! */
typedef enum {
  cache_ck_hit = 0,
  cache_ck_miss = 1,
  cache_ck_expired = 2,

  cache_ck_invalid,
} cache_ck_res_e;

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_ENUM_H

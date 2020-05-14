//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef LIBMIMIRCACHE_ENUM_H
#define LIBMIMIRCACHE_ENUM_H


#ifdef __cplusplus
extern "C"
{
#endif

// trace type
typedef enum {
  CSV_TRACE = 'c', BIN_TRACE = 'b', PLAIN_TXT_TRACE = 'p', VSCSI_TRACE = 'v', TWR_TRACE = 't',
} trace_type_t;

// obj_id type
typedef enum {
  OBJ_ID_NUM = 'l', OBJ_ID_STR = 'c'
} obj_id_type_t;


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_ENUM_H

#pragma once
//
// Created by Juncheng Yang on 3/19/20.
//

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <time.h>

#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/reader.h"

static inline GHashTable *create_hash_table(
    reader_t *reader, GDestroyNotify key_destroy_func_num,
    GDestroyNotify value_destroy_func_num, GDestroyNotify key_destroy_func_str,
    GDestroyNotify value_destroy_func_str) {
  GHashTable *hash_table;
  hash_table =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, key_destroy_func_num,
                            value_destroy_func_num);
  return hash_table;
}

// static inline GHashTable *create_hash_table_with_obj_id_type(
//     obj_id_type_e obj_id_type, GDestroyNotify key_destroy_func_num,
//     GDestroyNotify value_destroy_func_num, GDestroyNotify key_destroy_func_str,
//     GDestroyNotify value_destroy_func_str) {
//   GHashTable *hash_table;
//   if (obj_id_type == OBJ_ID_NUM) {
//     hash_table =
//         g_hash_table_new_full(g_int64_hash, g_direct_equal,
//                               key_destroy_func_num, value_destroy_func_num);
//   } else if (obj_id_type == OBJ_ID_STR) {
//     hash_table = g_hash_table_new_full(
//         g_str_hash, g_str_equal, key_destroy_func_str, value_destroy_func_str);
//   } else {
//     ERROR("cannot recognize obj_id_type %c\n", obj_id_type);
//     abort();
//   }
//   return hash_table;
// }

#ifdef __cplusplus
}
#endif

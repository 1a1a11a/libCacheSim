//
// Created by Juncheng Yang on 3/19/20.
//

#ifndef LIBMIMIRCACHE_UTILSINTERNAL_H
#define LIBMIMIRCACHE_UTILSINTERNAL_H


#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <time.h>
#include "../../include/mimircache/logging.h"


static inline GHashTable *create_hash_table(reader_t *reader,
                              GDestroyNotify key_destroy_func_num,
                              GDestroyNotify value_destroy_func_num,
                              GDestroyNotify key_destroy_func_str,
                              GDestroyNotify value_destroy_func_str){
  GHashTable *hash_table;
  if (reader->base->obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, key_destroy_func_num, value_destroy_func_num);
  } else if (reader->base->obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, key_destroy_func_str, value_destroy_func_str);
  } else {
    ERROR("cannot recognize reader obj_id_type %c\n", reader->base->obj_id_type);
    abort();
  }
  return hash_table;
}

static inline GHashTable *create_hash_table_with_obj_id_type(obj_id_t obj_id_type,
                                            GDestroyNotify key_destroy_func_num,
                                            GDestroyNotify value_destroy_func_num,
                                            GDestroyNotify key_destroy_func_str,
                                            GDestroyNotify value_destroy_func_str){
  GHashTable *hash_table;
  if (obj_id_type == OBJ_ID_NUM) {
    hash_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, key_destroy_func_num, value_destroy_func_num);
  } else if (obj_id_type == OBJ_ID_STR) {
    hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, key_destroy_func_str, value_destroy_func_str);
  } else {
    ERROR("cannot recognize obj_id_type %c\n", obj_id_type);
    abort();
  }
  return hash_table;
}

static inline void check_null(gpointer ptr, char* err_msg){
  if (ptr == NULL){
    ERROR("%s", err_msg);
    abort();
  }
}

static inline void check_false(gboolean in, char* err_msg){
  if (!in){
    ERROR("%s", err_msg);
    abort();
  }
}

static inline void print_progress(double perc){
  static double last_perc = 0;
  static time_t last_print_time = 0;
  time_t cur_time = time(NULL);

  if (perc - last_perc < 0.01 || cur_time - last_print_time < 60){
    last_perc = perc;
    last_print_time = cur_time;
    sleep(2);
    return;
  }
  if (last_perc != 0)
    fprintf(stderr, "\033[A\033[2K\r");
  fprintf(stderr, "%.2f%%\n", perc);
  last_perc = perc;
  last_print_time = cur_time;
}


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_UTILSINTERNAL_H

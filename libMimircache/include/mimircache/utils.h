//
// Created by Juncheng Yang on 11/16/19.
//

#ifndef MIMIRCACHE_UTILS_H
#define MIMIRCACHE_UTILS_H


#include "const.h"


/******************************* profiler ******************************/
typedef struct break_point {
  GArray *array;
  char mode; // r or v
  guint64 time_interval;
} break_point_t;


/******************************* cleaner ******************************/
//static void simple_key_value_destroyer(gpointer data) {
//  free(data);
//}
//
//static void simple_g_key_value_destroyer(gpointer data) {
//  g_free(data);
//}





#endif //MIMIRCACHE_UTILS_H

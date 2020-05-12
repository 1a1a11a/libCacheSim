//
// Created by Juncheng Yang on 4/23/20.
//

#ifndef LIBMIMIRCACHE_CACHEUTILS_H
#define LIBMIMIRCACHE_CACHEUTILS_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>
#include "../../include/mimircache/cacheObj.h"


/****************** find obj expiration related info ******************/
typedef struct {
  guint64 n_exp_obj;
  guint64 n_exp_byte;
  guint64 n_total_obj;
  guint64 n_total_byte;
  gint32 cur_time;
} exp_info_t;


static inline void _count_exp(gpointer key,
                              gpointer value,
                              gpointer user_data) {
  exp_info_t *exp_info = (exp_info_t*) user_data;
  slab_cache_obj_t *obj = (slab_cache_obj_t*) value;

  exp_info->n_total_byte += obj->obj_size;
  exp_info->n_total_obj += 1;

  if (obj->ttl < exp_info->cur_time){
    exp_info->n_exp_byte += obj->obj_size;
    exp_info->n_exp_obj += 1;
  }
}


static inline exp_info_t scan_for_expired_obj(GHashTable *hashtable, guint64 cur_time) {
  exp_info_t exp_info;
  exp_info.cur_time = cur_time;
  g_hash_table_foreach(hashtable, _count_exp, &exp_info);
  return exp_info;
}


#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_CACHEUTILS_H

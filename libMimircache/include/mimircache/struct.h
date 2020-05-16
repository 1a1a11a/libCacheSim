//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef LIBMIMIRCACHE_STRUCT_H
#define LIBMIMIRCACHE_STRUCT_H


#ifdef __cplusplus
extern "C" {
#endif


#include "../config.h"


// ######################################## cache obj #####################################
typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
#ifdef SUPPORT_TTL
  gint32 exp_time;
#endif
} cache_obj_t;

typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
  gint32 ttl;
} ttl_cache_obj_t;

typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
  gint32 ttl;
  gchar extra_data[32];
} cache_obj_ext_t;

typedef struct {
  gpointer obj_id_ptr;
  guint32 obj_size;
#ifdef SUPPORT_TTL
  gint32 exp_time;
#endif
  void *slab;
  gint32 item_pos_in_slab;
} slab_cache_obj_t;



#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_STRUCT_H

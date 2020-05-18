//
// Created by Juncheng Yang on 3/21/20.
//

#ifndef LIBMIMIRCACHE_CACHEALGUTILS_H
#define LIBMIMIRCACHE_CACHEALGUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif


#include "../../include/mimircache/cache.h"

// now use
struct exp_cnt{
  gint32 cur_time;
  guint64 n_obj;
  guint64 n_exp;
};

static inline void count_expiration1(gpointer data, gpointer user_data){
  cache_obj_t* cache_obj = data;
  struct exp_cnt *cnt = user_data;
  cnt->n_obj += 1;
  if (cache_obj->exp_time < cnt->cur_time){
    cnt->n_exp += 1;
  }
}

static inline void count_expiration2(gpointer data, gpointer user_data){
  slab_cache_obj_t* cache_obj = data;
  struct exp_cnt *cnt = user_data;
  cnt->n_obj += 1;
  if (cache_obj->exp_time < cnt->cur_time){
    cnt->n_exp += 1;
  }
}

static inline void count_expiration3(gpointer data, gpointer user_data){
  cache_obj_t* cache_obj = ((GList*) data)->data;
  struct exp_cnt *cnt = user_data;
  cnt->n_obj += 1;
  if (cache_obj->exp_time < cnt->cur_time){
    cnt->n_exp += 1;
  }
}

static inline void count_expiration4(gpointer data, gpointer user_data){
  slab_cache_obj_t* cache_obj = ((GList*) data)->data;
  struct exp_cnt *cnt = user_data;
  cnt->n_obj += 1;
  if (cache_obj->exp_time < cnt->cur_time){
    cnt->n_exp += 1;
  }
}

#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CACHEALGUTILS_H

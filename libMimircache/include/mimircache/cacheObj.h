//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_H
#define MIMIRCACHE_CACHEOBJ_H


//#define TRACK_EVICTION_AGE
#ifdef TRACK_EVICTION_AGE
#define TRACK_ACCESS_TIME
#endif


typedef struct {
  gpointer obj_id;
  guint32 size;
#ifdef TRACK_ACCESS_TIME
  // add this only when necessary, otherwise it will add a lot of memory overhead
  guint32 access_time;
#endif
  void *extra_data;
} cache_obj_t;


static inline void cacheobj_destroyer(gpointer data) {
  cache_obj_t *cache_obj = (cache_obj_t *) data;
  if (cache_obj->extra_data)
    g_free(cache_obj->extra_data);
  g_free(data);
}


#endif //MIMIRCACHE_CACHEOBJ_H

//
//  cache.h
//  mimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CACHE_H
#define CACHE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include "reader.h"
#include "const.h"
#include "macro.h"
#include "logging.h"
#include "utils.h"
#include "request.h"
#include "cacheObj.h"


struct cache_core {
//  cache_type type;      // need to deprecate this for allowing unknown cache replacement algorithm
  char cache_name[32];
  // virtual timestamp (added on 06/02/2019)
  guint64 ts;

  long size;
  guint64 used_size;
  char obj_id_type;     // l, c
  long long hit_count;
  long long miss_count;
  void *cache_init_params;
//  gboolean use_block_size;
//  guint64 block_size;

  struct cache *(*cache_init)(guint64, obj_id_t, void *);

  void (*destroy)(struct cache *);

  void (*destroy_unique)(struct cache *);

  gboolean (*add)(struct cache *, request_t *);

  gboolean (*check)(struct cache *, request_t *);

  void *(*get_cached_data)(struct cache *, request_t *);

  void (*update_cached_data)(struct cache *, request_t *, void *);

  void (*_insert)(struct cache *, request_t *);

  void (*_update)(struct cache *, request_t *);

  void (*_evict)(struct cache *, request_t *);

  gpointer (*evict_with_return)(struct cache *, request_t *);

  guint64 (*get_current_size)(struct cache *);         // get current size of used cache

  GHashTable *(*get_objmap)(struct cache *);     // get the hash map

  void (*remove_obj)(struct cache *, void *);

//  gboolean (*add_element_only)(struct cache *, request_t *);

//  gboolean (*add_element_withsize)(struct cache *, request_t *);
  // only insert(and possibly evict) or update, do not conduct any other
  // operation, especially for those complex algorithm


  break_point_t *bp;           // break points, same as the one in reader, just one more pointer
  guint64 bp_pos;              // the current location in bp->array

  /** Jason: need to remove shared struct in cache and move all shared struct into reader **/
  /** the fields below are not used any more, should be removed in the next major release **/
//  int record_level;  // 0 not debug, 1: record evictions, 2: compare to oracle
//  void *oracle;
//  void *eviction_array;     // Optimal Eviction Array, either guint64* or char**
//  guint64 eviction_array_len;
//  guint64 evict_err;      // used for counting
//  gdouble *evict_err_array;       // in each time interval, the eviction error array
};

typedef struct cache {
  struct cache_core *core;
  void *cache_params;
} cache_t;


extern cache_t *cache_init(char* cache_name, long long size, obj_id_t obj_id_type);
extern void cache_destroy(cache_t *cache);
extern void cache_destroy_unique(cache_t *cache);

static inline void* _get_cache_func_ptr(char* cache_alg_name, char* func_name){
  char full_func_name[256];
  sprintf(full_func_name, "%s_%s", cache_alg_name, func_name);
  void* func_ptr = dlsym(NULL, full_func_name);

  char *error;
  if ((error = dlerror()) != NULL) {
    WARNING("error loading %s %s\n", full_func_name, error);
    return NULL;
  } else {
    DEBUG("internal cache loaded %s\n", full_func_name);
    return func_ptr;
  }
}



#ifdef __cplusplus
}
#endif

#endif /* cache_h */

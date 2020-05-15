//
//  cache.h
//  libMimircache
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

#include "request.h"
#include "cacheObj.h"
#include "mem.h"

#include "../config.h"



struct cache;

typedef struct {
  gint64 cache_size;
  obj_id_type_t obj_id_type;
  gboolean support_ttl;
} common_cache_params_t;

struct cache_core {
  guint64 req_cnt;
  gint64 size;
  gint64 used_size;
  gboolean support_ttl;
  obj_id_type_t obj_id_type;
  mem_allocator_t mem_alloc;

  gboolean (*get)(struct cache *, request_t *);

  gboolean (*check)(struct cache *, request_t *);

  void (*_insert)(struct cache *, request_t *);

  void (*_update)(struct cache *, request_t *);

  void (*_evict)(struct cache *, request_t *);

  gpointer (*evict_with_return)(struct cache *, request_t *);

  GHashTable *(*get_objmap)(struct cache *);     // get the hash map

  cache_obj_t *(*get_cached_obj)(struct cache *, request_t *);

  void (*remove_obj)(struct cache *, void *);

  struct cache *(*cache_init)(common_cache_params_t, void *);

  void (*cache_free)(struct cache *);

  void *cache_specific_init_params;
  char cache_name[32];
};

struct cache_stat {
  gint64 req_cnt;
  gint64 req_byte;
  guint64 hit_cnt;
  guint64 hit_byte;
  guint64 hit_expired_cnt;
  guint64 hit_expired_byte;
};

typedef struct cache {
  struct cache_core core;
  void *cache_params;
  struct cache_stat stat;
} cache_t;


cache_t *cache_struct_init(const char* const cache_name, common_cache_params_t);
void cache_struct_free(cache_t *cache);
cache_t *create_cache_with_new_size(cache_t *old_cache, gint64 new_size);


#ifdef __cplusplus
}
#endif

#endif /* cache_h */

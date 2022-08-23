//
//  Optimal.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef Optimal_h
#define Optimal_h


#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/distUtils.h"
#include "utilsInternal.h"


#ifdef __cplusplus
extern "C"
{
#endif


struct Optimal_init_params {
  reader_t *reader;
  GArray *next_access;
  guint64 ts;
};

typedef struct Optimal_params Optimal_params_t;
typedef struct Optimal_init_params Optimal_init_params_t;


extern void _Optimal_insert(cache_t *Optimal, request_t *cp);

extern gboolean Optimal_check(cache_t *cache, request_t *cp);

extern void _Optimal_update(cache_t *Optimal, request_t *cp);

extern void _Optimal_evict(cache_t *Optimal, request_t *cp);

extern void *_Optimal_evict_with_return(cache_t *cache, request_t *cp);

extern gboolean Optimal_add(cache_t *cache, request_t *cp);

extern gboolean Optimal_add_only(cache_t *cache, request_t *cp);

extern void Optimal_destroy(cache_t *cache);

extern void Optimal_destroy_unique(cache_t *cache);

cache_t *Optimal_init(common_cache_params_t ccache_params, void *cache_specific_init_params);


#ifdef __cplusplus
}
#endif


#endif

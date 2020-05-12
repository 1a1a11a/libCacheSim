//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_PLUGIN_H
#define MIMIRCACHE_PLUGIN_H

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "cache.h"
#include "reader.h"


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * create a cache handler, using the cache replacement algorithm is baked into mimircache
 * @param cache_alg_name name of the cache replacement algorithm (case senstive)
 * @param cc_params general parameters that are used to initialize cache, such as cache_size, obj_id_type, support_ttl
 * @param params a cache eviction algorithm specific params
 * @return cache handler
 */
cache_t *create_cache(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params);

/**
 * create a cache handler, using the cache replacement algorithm is baked into mimircache
 * @param cache_alg_name name of the cache replacement algorithm (case senstive)
 * @param cc_params general parameters that are used to initialize cache, such as cache_size, obj_id_type, support_ttl
 * @param params a cache eviction algorithm specific params
 * @return cache handler
 */
cache_t *
create_cache_internal(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params);


/**
 * create a cache handler, using an external cache replacement algorithm, for now we assume the external cache replacment
 * algorithm is compiled into a shared library and stored in the working directory and the name is <alg>.so where alg is
 * the name of the cache replacement algorithm passed to this function
 * @param cache_alg_name name of the cache replacement algorithm (case senstive)
 * @param cc_params general parameters that are used to initialize cache, such as cache_size, obj_id_type, support_ttl
 * @param params a cache eviction algorithm specific params
 * @return cache handler
 */
cache_t *
create_cache_external(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params);

#ifdef __cplusplus
}
#endif


#endif //MIMIRCACHE_PLUGIN_H

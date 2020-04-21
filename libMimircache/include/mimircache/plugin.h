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
 * @param size cache size in unit of byte
 * @param data_type the type of object id in the trace, can be either OBJ_ID_NUM or OBJ_ID_STR
 * @param params the parameter pass to cache initialization
 * @return cache handler
 */
cache_t *create_cache(const char *const cache_alg_name, uint64_t size, obj_id_type_t obj_id_type, void *params);

/**
 * create a cache handler, using the cache replacement algorithm is baked into mimircache
 * @param cache_alg_name name of the cache replacement algorithm (case senstive)
 * @param size cache size in unit of byte
 * @param data_type the type of object id in the trace, can be either OBJ_ID_NUM or OBJ_ID_STR
 * @param params the parameter pass to cache initialization
 * @return cache handler
 */
cache_t *
create_cache_internal(const char *const cache_alg_name, uint64_t size, obj_id_type_t obj_id_type, void *params);


/**
 * create a cache handler, using an external cache replacement algorithm, for now we assume the external cache replacment
 * algorithm is compiled into a shared library and stored in the working directory and the name is <alg>.so where alg is
 * the name of the cache replacement algorithm passed to this function
 * @param cache_alg_name name of the cache replacement algorithm (case senstive)
 * @param size cache size in unit of byte
 * @param data_type the type of object id in the trace, can be either OBJ_ID_NUM or OBJ_ID_STR
 * @param params the parameter pass to cache initialization
 * @return cache handler
 */
cache_t *
create_cache_external(const char *const cache_alg_name, uint64_t size, obj_id_type_t obj_id_type, void *params);

#ifdef __cplusplus
}
#endif


#endif //MIMIRCACHE_PLUGIN_H

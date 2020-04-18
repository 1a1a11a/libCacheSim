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


GHashTable *create_hash_table_cache_alg();

void queue_node_destroyer(gpointer data);


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CACHEALGUTILS_H

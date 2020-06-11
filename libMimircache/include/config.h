//
// Created by Juncheng Yang on 5/10/20.
//

#ifndef LIBMIMIRCACHE_CONFIG_H
#define LIBMIMIRCACHE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//#pragma GCC optimize("Ofast")
//#pragma GCC target("avx,avx2,fma")

#include "mimircache/const.h"


#ifndef MIMIR_LOGLEVEL
#define MIMIR_LOGLEVEL INFO_LEVEL
#endif

#ifndef HEAP_ALLOCATOR
#define HEAP_ALLOCATOR HEAP_ALLOCATOR_MALLOC
#endif

#ifndef HASH_TYPE
#define HASH_TYPE MURMUR3
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* for sched in utils.h */
#endif


#ifndef HASHTABLE_TYPE
#define HASHTABLE_TYPE CHAINED_HASHTABLE
#endif

#define HASHTABLE_VER 1

#define SUPPORT_TTL
#define SUPPORT_SLAB_AUTOMOVE


#define HASH_POWER_DEFAULT 20
#define CHAINED_HASHTABLE_EXPAND_THRESHOLD 1


//#undef SUPPORT_TTL
//#undef USE_CUSTOME_MEM_ALLOCATOR


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CONFIG_H

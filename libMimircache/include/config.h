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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* for sched in utils.h */
#endif



#ifndef MIMIR_LOGLEVEL
#define MIMIR_LOGLEVEL INFO_LEVEL
#endif

#ifndef HEAP_ALLOCATOR
#define HEAP_ALLOCATOR HEAP_ALLOCATOR_MALLOC
#endif

#ifndef HASH_TYPE
//#define HASH_TYPE IDENTITY
#define HASH_TYPE MURMUR3
//#define HASH_TYPE WYHASH
//#define HASH_TYPE XXHASH3
#endif


#ifndef HASHTABLE_TYPE
#define HASHTABLE_TYPE CHAINED_HASHTABLE
#endif

#ifndef HASHTABLE_VER
#define HASHTABLE_VER 1
#endif

#ifndef HASH_POWER_DEFAULT
#define HASH_POWER_DEFAULT 24
#endif

#ifndef CHAINED_HASHTABLE_EXPAND_THRESHOLD
#define CHAINED_HASHTABLE_EXPAND_THRESHOLD 1
#endif


#
#include <sys/mman.h>
#ifdef MADV_HUGEPAGE
#define USE_HUGEPAGE
#endif



#define SUPPORT_TTL
#define SUPPORT_SLAB_AUTOMOVE




#include <stdint.h>
typedef uint64_t obj_id_t;



#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CONFIG_H

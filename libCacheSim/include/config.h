//
// Created by Juncheng Yang on 5/10/20.
//

#ifndef libCacheSim_CONFIG_H
#define libCacheSim_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

//#pragma GCC optimize("Ofast")
//#pragma GCC target("avx,avx2,fma")

#include "libCacheSim/const.h"  // needed for hash type

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for sched in utils.h */
#endif

#ifndef HEAP_ALLOCATOR
#define HEAP_ALLOCATOR HEAP_ALLOCATOR_MALLOC
#endif

#ifndef HASH_TYPE
//#define HASH_TYPE IDENTITY
//#define HASH_TYPE MURMUR3
//#define HASH_TYPE WYHASH
#define HASH_TYPE XXHASH3
#else
#error "HASH_TYPE is defined"
#endif

#ifndef HASHTABLE_TYPE
#define HASHTABLE_TYPE CHAINED_HASHTABLEV2
#endif

#ifndef HASH_POWER_DEFAULT
#define HASH_POWER_DEFAULT 23
#endif

#ifndef CHAINED_HASHTABLE_EXPAND_THRESHOLD
#define CHAINED_HASHTABLE_EXPAND_THRESHOLD 2
#endif

#include <sys/mman.h>
#ifndef MADV_HUGEPAGE
#undef USE_HUGEPAGE
#endif

// #define TRACK_EVICTION_V_AGE
// #define TRACK_DEMOTION
// #define TRACK_CREATE_TIME

#if defined(TRACK_EVICTION_V_AGE) || defined(TRACK_DEMOTION) || \
    defined(TRACK_CREATE_TIME)
#define CURR_TIME(cache, req) (cache->n_req)
#elif defined(TRACK_EVICTION_R_AGE)
#define CURR_TIME(cache, req) (req->clock_time)
#endif


#include <stdint.h>
typedef uint64_t obj_id_t;

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_CONFIG_H

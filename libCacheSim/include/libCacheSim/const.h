#pragma once

//
//  const.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CONST_H
#define CONST_H

#ifdef __cplusplus
extern "C" {
#endif

#define HEAP_ALLOCATOR_G_NEW 0xa10
#define HEAP_ALLOCATOR_G_SLICE_NEW 0xa20
#define HEAP_ALLOCATOR_MALLOC 0xa30
#define HEAP_ALLOCATOR_ALIGNED_MALLOC 0xa40

#define MURMUR3 0xb10
#define XXHASH 0xb20
#define XXHASH3 0xb21
#define WYHASH 0xb30  // not significantly faster than MURMUR3
#define IDENTITY 1

#define CHAINED_HASHTABLE 0xc1
#define CUCKOO_HASHTABLE 0xc2

#define MEM_ALIGN_SIZE 128

#define NORMAL "\x1B[0m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"

#define VVVERBOSE_LEVEL 3
#define VVERBOSE_LEVEL 4
#define VERBOSE_LEVEL 5
#define DEBUG_LEVEL 6
#define INFO_LEVEL 7
#define WARN_LEVEL 8
#define SEVERE_LEVEL 9

// this is correct, to change to this, need to update test
#define KiB 1024LL
#define MiB 1048576LL
#define GiB 1073741824LL
#define TiB 1099511627776LL

#define KB 1000L
#define MB 1000000L
#define GB 1000000000L
#define TB 1000000000000L

#define MAX_REUSE_DISTANCE INT64_MAX

#ifdef __cplusplus
}
#endif

#endif

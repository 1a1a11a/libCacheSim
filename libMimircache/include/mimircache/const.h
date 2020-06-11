//
//  const.h
//  libMimircache
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
#define WYHASH 0xb31   // not significantly faster than MURMUR3
#define IDENTITY 1


#define CHAINED_HASHTABLE 0xc1
#define CUCKOO_HASHTABLE 0xc2


#define MEM_ALIGN_SIZE 128

#define MAX_OBJ_ID_LEN 1024
#define MAX_FILE_PATH_LEN 1024
#define MAX_LINE_LEN 1024 * 8
#define MAX_BIN_FMT_STR_LEN 128


#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif


#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"


#define DEBUG3_LEVEL 4
#define DEBUG2_LEVEL 5
#define DEBUG_LEVEL   6
#define INFO_LEVEL    7
#define WARNING_LEVEL 8
#define SEVERE_LEVEL  9




//#define KB 1024L
//#define MB 1048576L
//#define GB 1073741824L
//#define TB 1099511627776L
//
//#define KiB 1000L
//#define MiB 1000000L
//#define GiB 1000000000L
//#define TiB 1000000000000L


// this is correct, to change to this, need to update test
#define KiB 1024L
#define MiB 1048576L
#define GiB 1073741824L
#define TiB 1099511627776L

#define KB 1000L
#define MB 1000000L
#define GB 1000000000L
#define TB 1000000000000L

//#define DEFAULT_SECTOR_SIZE                             512






#ifdef __cplusplus
}
#endif

#endif

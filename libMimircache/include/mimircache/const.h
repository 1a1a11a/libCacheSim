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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* for sched in utils.h */
#endif

//#undef __DEBUG__
//#undef _DEBUG

// if TTL is not needed, disable this to reduce memory usage
#define SUPPORT_TTL


#define MAX_OBJ_ID_LEN 1024
#define MAX_FILE_PATH_LEN 1024

#define MAX_LINE_LEN 1024 * 8

#define MAX_BIN_FMT_STR_LEN 128




#define KB 1024L
#define MB 1048576L
#define GB 1073741824L
#define TB 1099511627776L

#define KiB 1000L
#define MiB 1000000L
#define GiB 1000000000L
#define TiB 1000000000000L


// this is correct, to change to this, need to update test
//#define KiB 1024L
//#define MiB 1048576L
//#define GiB 1073741824L
//#define TiB 1099511627776L
//
//#define KB 1000L
//#define MB 1000000L
//#define GB 1000000000L
//#define TB 1000000000000L

//#define DEFAULT_SECTOR_SIZE                             512



#ifdef __cplusplus
}
#endif

#endif

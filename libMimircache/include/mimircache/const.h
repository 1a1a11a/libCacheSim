//
//  const.h
//  mimircache
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

#undef __DEBUG__
#undef _DEBUG




#define MAX_OBJ_ID_LEN 1024
#define MAX_FILE_PATH_LEN 1024

//#define BINARY_FMT_MAX_LEN 32
#define MAX_LINE_LEN 1024 * 1024


typedef enum {
  UNKNOWN_DIST = 0, REUSE_DIST = 1, FUTURE_RD = 2, LAST_DIST = 3, NEXT_DIST=4, REUSE_TIME = 5,
} dist_t;

// obj_id type
typedef enum { OBJ_ID_NUM = 'l', OBJ_ID_STR = 'c' } obj_id_t;


//#define DEFAULT_SECTOR_SIZE                             512



#ifdef __cplusplus
}
#endif

#endif

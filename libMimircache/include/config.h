//
// Created by Juncheng Yang on 5/10/20.
//

#ifndef LIBMIMIRCACHE_CONFIG_H
#define LIBMIMIRCACHE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MIMIR_LOGLEVEL
#define MIMIR_LOGLEVEL 7
#endif

#define SUPPORT_TTL
//#define USE_CUSTOME_MEM_ALLOCATOR
#define USE_GLIB_SLICE_ALLOCATOR

//#undef SUPPORT_TTL
//#undef USE_CUSTOME_MEM_ALLOCATOR


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CONFIG_H

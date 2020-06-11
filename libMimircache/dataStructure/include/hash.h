//
// Created by Juncheng Yang on 6/7/20.
//

#ifndef LIBMIMIRCACHE_HASH_H
#define LIBMIMIRCACHE_HASH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "murmur3.h"
#include "../../include/config.h"


typedef enum{
  HASH_SEED0 = 0x12345678,
  HASH_SEED1 = 0x12344321,
  HASH_SEED2 = 0x23456789,
  HASH_SEED3 = 0x3456789A,
  HASH_SEED4 = 0x456789AB,
  HASH_SEED5 = 0x56789ABC,
  HASH_SEED6 = 0x6789ABCD,
  HASH_SEED7 = 0x789ABCDE,
}hash_seeds;


#if HASH_TYPE == XXHASH
#error xxhash here
#include "xxhash.h"
#endif


//static inline void get_hash_value_str_64(const void *key, int len, uint64 *out){
//#if HASH_TYPE == MURMUR3
//  MurmurHash3_x64_64(key, len, HASH_SEED0, out);
//#elif HASH_TYPE == XXHASH
//  XXHash3_x64_64(key, len, HASH_SEED0, out);
//#else
//  #error "unknown"
//#endif
//}

//static inline void get_hash_value_int_64(const int64_t key, uint64 *out){
//  return get_hash_value_str_64((char*) &key, 8, out);
//}

#if HASH_TYPE == MURMUR3
#define get_hash_value_int_64(key) (MurmurHash3_x64_64(key, 8, HASH_SEED0))
#elif HASH_TYPE == XXHASH
#define get_hash_value_int_64(key) (XXHash3_x64_64(key, 8, HASH_SEED0));
#else
  #error "unknown"
#endif


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_HASH_H

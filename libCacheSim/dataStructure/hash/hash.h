//
// Created by Juncheng Yang on 6/7/20.
//

#ifndef libCacheSim_HASH_H
#define libCacheSim_HASH_H

#ifdef __cplusplus
extern "C"
{
#endif

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
} hash_seeds;


#if HASH_TYPE == MURMUR3
  #include "murmur3.h"
  #define get_hash_value_int_64(key_p) (MurmurHash3_x64_64(key_p, sizeof(obj_id_t), HASH_SEED0))
  #define get_hash_value_str(key, key_len) (MurmurHash3_x64_64((void*)(key), key_len, HASH_SEED0))
  #error "murmur hash enabled?"
#elif HASH_TYPE == XXHASH
#define XXH_INLINE_ALL
  #include "xxhash.h"
  #define get_hash_value_int_64(key_p) (XXH64((void*)(key_p), sizeof(obj_id_t), HASH_SEED0))
  #define get_hash_value_str(key, key_len) (uint64_t) (XXH64((void*)(key), key_len, HASH_SEED0))
#elif HASH_TYPE == XXHASH3
  #define XXH_INLINE_ALL
  #include "xxhash.h"
  #include "xxh3.h"
  #define get_hash_value_int_64(key_p) (uint64_t) (XXH3_64bits((void*)(key_p), sizeof(obj_id_t)))
  #define get_hash_value_str(key, key_len) (uint64_t) (XXH3_64bits((void*)(key), key_len))
#elif HASH_TYPE == WYHASH
  #include "wyhash.h"
  #define get_hash_value_int_64(key_p) (uint64_t) (wyhash64(*(obj_id_t*)key_p, HASH_SEED0))
  #define get_hash_value_str(key, key_len) abort()
#elif HASH_TYPE == IDENTITY
  #define get_hash_value_int_64(key) (*key)
  #define get_hash_value_str(key, key_len) abort()
#else
  #error "unknown hash"
#endif

//(size_t) XXH64(src, srcSize, 0)
//(size_t) XXH3_64bits(src, srcSize)


#ifdef __cplusplus
}
#endif


#endif //libCacheSim_HASH_H

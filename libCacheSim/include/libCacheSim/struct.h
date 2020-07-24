//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef libCacheSim_STRUCT_H
#define libCacheSim_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config.h"
#include "enum.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * I had a hard time deciding which type of hashtable design I should adopt,
 * cuckoo hash or chained, I was inclined to cuckoo hash due to its CPU cache
 * friendliness, less memory indirection and memory efficiency however, given
 * that if the occupancies rate is low, then more space will be wasted, and of
 * course, it is harder to implement, there is no existing code base I can
 * borrow, so for now, I plan to go with chained hash table
 *
 */
// ############################## cache obj ###################################

struct cache_obj;
typedef struct cache_obj {
  struct cache_obj *hash_next;
  struct cache_obj *list_next;
  struct cache_obj *list_prev;
  union {
    void *extra_metadata_ptr;
    uint64_t extra_metadata_u64;
    uint8_t extra_metadata_u8[8];
  };
  obj_id_t obj_id_int;
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  uint32_t exp_time;
#endif
} cache_obj_t;

typedef struct {
  obj_id_t obj_id_int;
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  uint32_t exp_time;
#endif
#ifdef SUPPORT_SLAB_AUTOMOVE
  uint32_t access_time;
#endif
  void *slab;
  int32_t item_pos_in_slab;
} slab_cache_obj_t;

/* need to optimize this for CPU cacheline */
typedef struct {
  uint64_t real_time; /* use uint64_t because vscsi uses microsec timestamp */
  uint64_t hv;        /* hash value, used when offloading hash to reader */
  obj_id_t obj_id_int;
  uint32_t obj_size;
  int32_t ttl;
  req_op_e op;
  uint64_t extra_field1;
  uint64_t extra_field2;
  bool valid;        /* indicate whether request is valid request
                      * it is invlalid if the trace reaches the end */
} request_t;

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_STRUCT_H

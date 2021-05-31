//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef libCacheSim_STRUCT_H
#define libCacheSim_STRUCT_H

#include "../config.h"
#include "enum.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

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
  obj_id_t obj_id;
  union {
    struct cache_obj *list_prev;
    double score;
  };
  union {
    struct cache_obj *list_next;
    int64_t next_access_ts;
  };
  uint32_t obj_size;
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  uint32_t exp_time;
#endif
#if defined(ENABLE_LLSC)
  union {
    struct {
      void *segment;
      int64_t next_access_ts;
      int32_t LLSC_freq;
      int32_t last_access_rtime;
      int16_t idx_in_segment;
      int16_t active : 2;
      int16_t in_cache : 2;
      int16_t seen_after_snapshot : 2;
//      int16_t n_merged : 12;  /* how many times it has been merged */
    } LSC;
#endif
    int64_t freq;
    void *extra_metadata_ptr;
    uint64_t extra_metadata_u64;
    uint8_t extra_metadata_u8[8];
  };
  union {
    int64_t last_access_rtime;
    int64_t last_access_vtime;
    void *extra_metadata2_ptr;
    uint64_t extra_metadata2_u64;
    uint8_t extra_metadata2_u8[8];
  };
#ifdef TRACK_EVICTION_AGE
#endif
} __attribute__((packed)) cache_obj_t;

typedef struct {
  obj_id_t obj_id;
  uint32_t obj_size;
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  uint32_t exp_time;
#endif
  //#ifdef SUPPORT_SLAB_AUTOMOVE
  uint32_t access_time;
  //#endif
  void *slab;
  int32_t item_pos_in_slab;
} slab_cache_obj_t;

/* need to optimize this for CPU cacheline */
typedef struct {
  uint64_t real_time; /* use uint64_t because vscsi uses microsec timestamp */
  uint64_t hv;        /* hash value, used when offloading hash to reader */
  union {
    obj_id_t obj_id;
    obj_id_t obj_id;
  };
  uint32_t obj_size;
  int32_t ttl;
  req_op_e op;

  int64_t next_access_ts;
  struct {
    uint64_t key_size : 16;
    uint64_t val_size : 48;
  };

  union {
    int32_t customer_id;
    int32_t app_id;
  };
  int32_t content_type;
  int32_t bucket_id;

  //  int64_t extra_field1;
  //  int64_t extra_field2;

  uint64_t n_req;

  bool valid; /* indicate whether request is valid request
                      * it is invlalid if the trace reaches the end */
} request_t;

#ifdef __cplusplus
}
#endif

#endif// libCacheSim_STRUCT_H

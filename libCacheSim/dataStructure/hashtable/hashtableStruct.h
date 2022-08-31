//
// Created by Juncheng Yang on 6/7/20.
//

#ifndef libCacheSim_HASHTABLESTRUCT_H
#define libCacheSim_HASHTABLESTRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "../../include/libCacheSim/cacheObj.h"

#define hashsize(n) ((uint64_t)1 << (uint16_t)(n))
#define hashsizeULL(n) ((unsigned long long)1 << (uint16_t)(n))
#define hashmask(n) (hashsize(n) - 1)

typedef void (*hashtable_iter)(cache_obj_t *cache_obj, void *user_data);

typedef struct hashtable {
  union {
    cache_obj_t *table;
    cache_obj_t **ptr_table;
    uint64_t *btable;
  };
  uint64_t n_obj;
  uint16_t hashpower;
  bool external_obj; /* whether the object should be allocated by hash table,
                        this should be true most of the time */
  union {
    // used for hashtable V1, these cache_obj pointers are used by external
    // modules, so if hashtable needs to move the obj, their pointer need to be
    // updated as well
    struct {
      cache_obj_t ***monitored_ptrs;
      uint16_t n_monitored_ptrs;
      uint16_t n_allocated_ptrs;
    };
    void *extra_data;
  };
} hashtable_t;

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_HASHTABLESTRUCT_H

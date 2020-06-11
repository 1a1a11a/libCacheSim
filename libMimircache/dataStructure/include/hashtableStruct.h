//
// Created by Juncheng Yang on 6/7/20.
//

#ifndef LIBMIMIRCACHE_HASHTABLESTRUCT_H
#define LIBMIMIRCACHE_HASHTABLESTRUCT_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include "../../include/mimircache/struct.h"


#define hashsize(n) ((uint64_t)1<<(uint16_t)(n))
#define hashsizeULL(n) ((unsigned long long)1<<(uint16_t)(n))
#define hashmask(n) (hashsize(n)-1)


typedef uint64_t (*key_to_hv_func_ptr)(void *ptr);

typedef bool (*key_cmp_func_ptr)(void *, void *);

typedef void (*hashtable_iter)(cache_obj_t* cache_obj, void* user_data);


typedef struct {
  union{
    cache_obj_t *table;
    cache_obj_t **table_twodim;
    cache_obj_t **ptr_table;
  };
  uint64_t n_cur_item;    // current occupied size
  uint16_t hash_power;
  union{
    // used for hashtable V1, these cache_obj pointers are used by external modules, so
    // if hashtable needs to move the obj, their pointer need to be updated as well
    struct{
      cache_obj_t ***monitored_ptrs;
      uint16_t n_monitored_ptrs;
      uint16_t n_allocated_ptrs;
    };
    void* extra_data;
  };
} hashtable_t;


#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_HASHTABLESTRUCT_H

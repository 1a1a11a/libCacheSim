//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef LIBMIMIRCACHE_STRUCT_H
#define LIBMIMIRCACHE_STRUCT_H


#ifdef __cplusplus
extern "C" {
#endif


#include "../config.h"
//#include "glib.h"
#include "enum.h"
#include <stdint.h>
#include <stdbool.h>


/**
 * I had a hard time deciding which type of hashtable design I should adopt, cuckoo hash or chained,
 * I was inclined to cuckoo hash due to its CPU cache friendliness, less memory indirection and memory efficiency
 * however, given that if the occupancies rate is low, then more space will be wasted,
 * and of course, it is harder to implement, there is no existing code base I can borrow,
 * so for now, I plan to go with chained hash table
 *
 */
// ######################################## cache obj #####################################
typedef struct {
  void* obj_id_ptr;
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  uint32_t exp_time;
#endif
} old_cache_obj_t;

struct cache_obj;
typedef struct cache_obj{
  uint64_t obj_id_int;
  struct cache_obj* hash_next;
  struct cache_obj* list_next;
  union{
    struct cache_obj* list_prev;
    void* extra_metadata;
  };
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  int32_t exp_time;
#endif
} new_cache_obj_t;

typedef new_cache_obj_t cache_obj_t;


typedef struct {
  union{
    uint64_t obj_id_int;
    char* obj_id_str;
  };
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  int32_t exp_time;
#endif
  void* next;       // can be used for LRU, FIFO
} cache_obj_ext_t;


typedef struct {
  uint64_t obj_id_int;
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  int32_t exp_time;
#endif
#ifdef SUPPORT_SLAB_AUTOMOVE
  int32_t access_time;
#endif
  void *slab;
  int32_t item_pos_in_slab;
} slab_cache_obj_t;



/* need to optimize this for CPU cacheline */
typedef struct {
  uint64_t real_time;         // use int64 because vscsi uses millisec timestamp
  uint64_t obj_id_int;
  uint32_t obj_size;
#ifdef SUPPORT_TTL
  int32_t ttl;
#endif
  req_op_e op;
  bool valid;   // used to indicate whether current request is a valid request and end of trace
  /* leave small field at the end to reduce padding */
} request_t;



#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_STRUCT_H

//
// Created by Juncheng Yang on 6/1/20.
//

#ifndef LIBMIMIRCACHE_HASHTABLE_H
#define LIBMIMIRCACHE_HASHTABLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/mimircache/cacheObj.h"
#include "../../include/mimircache/request.h"


typedef uint32_t (*key_to_hv_func_ptr)(void* ptr);
typedef bool (*key_cmp_func_ptr)(void*, void*);

typedef struct {
  void *table;
  uint64_t max_item;
  uint64_t n_cur_item;    // current occupied size
  // used to calculate the hash value of object
  key_to_hv_func_ptr key_to_hv;
  key_cmp_func_ptr key_cmp;

} hashtable_t;

hashtable_t *create_hashtable(uint64_t hashtable_size);

void *find_in_hashtable(hashtable_t *ht, uint64_t hv);

void add_to_hashtable(hashtable_t *ht, uint64_t hv, void *ptr);










/* function made for simulator */
cache_obj_t *find_cache_obj_in_hashtable(hashtable_t *ht, request_t *req);

void add_cache_obj_to_hashtable(hashtable_t *ht, cache_obj_t *cache_obj);



#ifdef __cplusplus
}
#endif
#endif //LIBMIMIRCACHE_HASHTABLE_H

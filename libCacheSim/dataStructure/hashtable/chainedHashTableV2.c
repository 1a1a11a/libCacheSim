//
// Created by Juncheng Yang on 6/9/20.
//


#ifdef __cplusplus
extern "C"
{
#endif

#include "chainedHashTableV2.h"
#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/macro.h"
#include "../../utils/include/mymath.h"
#include "../hash/hash.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>

#define OBJ_EMPTY(cache_obj) ((cache_obj)->obj_size == 0)
#define NEXT_OBJ(cur_obj) (((cache_obj_t*) (cur_obj))->hash_next)

void _chained_hashtable_expand_v2(hashtable_t *hashtable);

/************************ helper func ************************/
static inline cache_obj_t *_last_obj_in_bucket(hashtable_t *hashtable,
                                               uint64_t hv) {
  cache_obj_t *cur_obj_in_bucket = hashtable->ptr_table[hv];
  while (cur_obj_in_bucket->hash_next) {
    cur_obj_in_bucket = cur_obj_in_bucket->hash_next;
  }
  return cur_obj_in_bucket;
}

static inline void add_to_bucket(hashtable_t *hashtable,
                                 cache_obj_t *cache_obj) {
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id_int)
      & hashmask(hashtable->hash_power);
  if (hashtable->ptr_table[hv] == NULL) {
    hashtable->ptr_table[hv] = cache_obj;
    return;
  }
  cache_obj_t *cur_ptr = hashtable->ptr_table[hv];
  while (cur_ptr->hash_next != NULL)
    cur_ptr = cur_ptr->hash_next;

  cur_ptr->hash_next = cache_obj;
}

static inline void foreach_free_obj(cache_obj_t *cache_obj, void *user_data) {
  my_free(sizeof(cache_obj_t), cache_obj);
}

/************************ hashtable func ************************/
hashtable_t *create_chained_hashtable_v2(const uint16_t hash_power) {
  hashtable_t *hashtable = my_malloc(hashtable_t);
  hashtable->ptr_table = my_malloc_n(cache_obj_t*, hashsize(hash_power));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table, sizeof(cache_obj_t*)*hashsize(hashtable->hash_power), MADV_HUGEPAGE);
#endif
  memset(hashtable->ptr_table, 0, hashsize(hash_power) * sizeof(cache_obj_t *));
  hashtable->hash_power = hash_power;
  hashtable->n_cur_item = 0;
  return hashtable;
}

static inline cache_obj_t *chained_hashtable_find_obj_id_v2(hashtable_t *hashtable,
                                              obj_id_t obj_id) {
  cache_obj_t *cache_obj = NULL;
  uint64_t hv = get_hash_value_int_64(&obj_id);
  hv = hv & hashmask(hashtable->hash_power);
  cache_obj = hashtable->ptr_table[hv];

  while (cache_obj) {
    if (cache_obj->obj_id_int == obj_id) {
      return cache_obj;
    }
    cache_obj = cache_obj->hash_next;
  }
  return cache_obj;
}

cache_obj_t *chained_hashtable_find_v2(hashtable_t *hashtable, request_t *req) {
  return chained_hashtable_find_obj_id_v2(hashtable, req->obj_id_int);
}

cache_obj_t *chained_hashtable_find_obj_v2(hashtable_t *hashtable,
                                        cache_obj_t *obj_to_find) {
  return chained_hashtable_find_obj_id_v2(hashtable, obj_to_find->obj_id_int);
}

/* the user needs to make sure the added object is not in the hash table */
cache_obj_t *chained_hashtable_insert_v2(hashtable_t *hashtable,
                                         request_t *req) {
  if (hashtable->n_cur_item > (uint64_t) (hashsize(hashtable->hash_power)
      * CHAINED_HASHTABLE_EXPAND_THRESHOLD))
    _chained_hashtable_expand_v2(hashtable);

  cache_obj_t *new_cache_obj = create_cache_obj_from_request(req);
  add_to_bucket(hashtable, new_cache_obj);
  hashtable->n_cur_item += 1;
  return new_cache_obj;
}

/* you need to free the extra_metadata before deleting from hash table */
void chained_hashtable_delete_v2(hashtable_t *hashtable,
                                 cache_obj_t *cache_obj) {
  hashtable->n_cur_item -= 1;
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id_int)
      & hashmask(hashtable->hash_power);
  if (hashtable->ptr_table[hv] == cache_obj) {
    hashtable->ptr_table[hv] = cache_obj->hash_next;
    return;
  }

  cache_obj_t *cur_obj = hashtable->ptr_table[hv];
  while (cur_obj->hash_next != cache_obj && cur_obj != NULL)
    cur_obj = cur_obj->hash_next;
  assert(cur_obj != NULL);
  cur_obj->hash_next = cache_obj->hash_next;
  free_cache_obj(cache_obj);
}

cache_obj_t *chained_hashtable_rand_obj_v2(hashtable_t *hashtable) {
  uint64_t pos = next_rand() & hashmask(hashtable->hash_power);
  while (&hashtable->table[pos] == NULL)
    pos = next_rand() & hashmask(hashtable->hash_power);
  return &hashtable->table[pos];
}

void chained_hashtable_foreach_v2(hashtable_t *hashtable,
                                  hashtable_iter iter_func,
                                  void *user_data) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hash_power); i++) {
    cur_obj = hashtable->ptr_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      iter_func(cur_obj, user_data);
      cur_obj = next_obj;
    }
  }
}

void free_chained_hashtable_v2(hashtable_t *hashtable) {
  chained_hashtable_foreach_v2(hashtable, foreach_free_obj, NULL);
  my_free(sizeof(cache_obj_t *) * hashsize(hashtable->hash_power),
          hashtable->ptr_table);
  my_free(sizeof(hashtable_t), hashtable);
}

/* grows the hashtable to the next power of 2. */
void _chained_hashtable_expand_v2(hashtable_t *hashtable) {
  cache_obj_t **old_table = hashtable->ptr_table;
  hashtable->ptr_table =
      my_malloc_n(cache_obj_t*, hashsize(++hashtable->hash_power));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table, sizeof(cache_obj_t*)*hashsize(hashtable->hash_power), MADV_HUGEPAGE);
#endif
  memset(hashtable->ptr_table,
         0,
         hashsize(hashtable->hash_power) * sizeof(cache_obj_t *));
  ASSERT_NOT_NULL(hashtable->ptr_table,
                  "unable to grow hashtable to size %llu\n",
                  hashsizeULL(hashtable->hash_power));

  VERBOSE("hashtable resized from %llu to %llu\n",
          hashsizeULL((uint16_t) (hashtable->hash_power - 1)),
          hashsizeULL(hashtable->hash_power));

  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize((uint16_t) (hashtable->hash_power - 1));
       i++) {
    cur_obj = old_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      cur_obj->hash_next = NULL;
      add_to_bucket(hashtable, cur_obj);
      cur_obj = next_obj;
    }
  }
  my_free(sizeof(cache_obj_t) * hashsize(hashtable->hash_power), old_table);
}

void check_hashtable_integrity_v2(hashtable_t *hashtable) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hash_power); i++) {
    cur_obj = hashtable->ptr_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      assert(i == (get_hash_value_int_64(&cur_obj->obj_id_int)
          & hashmask(hashtable->hash_power)));
      cur_obj = next_obj;
    }
  }
}

void check_hashtable_integrity2_v2(hashtable_t *hashtable, cache_obj_t *head) {
  cache_obj_t *cur_obj = head, *cur_obj_in_hashchain;
  uint64_t hv;
  while (cur_obj) {
    hv = get_hash_value_int_64(&cur_obj->obj_id_int)
        & hashmask(hashtable->hash_power);
    cur_obj_in_hashchain = hashtable->ptr_table[hv];
    while (cur_obj_in_hashchain && cur_obj != cur_obj_in_hashchain)
      cur_obj_in_hashchain = cur_obj_in_hashchain->hash_next;
    assert(cur_obj_in_hashchain == cur_obj);
    cur_obj = cur_obj->list_next;
  }
}

#ifdef __cplusplus
}
#endif






//
// Created by Juncheng Yang on 6/7/20.
//


#ifdef __cplusplus
extern "C"
{
#endif

#include "chainedHashTable.h"
#include "../hash/hash.h"
#include "hashtableStruct.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/macro.h"
#include "mymath.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define OBJ_EMPTY(cache_obj) ((cache_obj)->obj_size == 0)

/************************ helper func ************************/
static inline cache_obj_t *_last_obj_in_bucket(hashtable_t *hashtable, uint64_t hv) {
  cache_obj_t *cur_obj_in_bucket = &hashtable->table[hv];
  while (cur_obj_in_bucket->hash_next) {
    cur_obj_in_bucket = cur_obj_in_bucket->hash_next;
  }
  return cur_obj_in_bucket;
}

static inline void update_monitored_ptr(hashtable_t *hashtable, cache_obj_t *new_obj, cache_obj_t *old_obj) {
  for (uint16_t i = 0; i < hashtable->n_monitored_ptrs; i++) {
    if (*(hashtable->monitored_ptrs[i]) == old_obj){
      *(hashtable->monitored_ptrs[i]) = new_obj;
    }
  }
}

static inline void _move_obj_to_new_loc(hashtable_t *hashtable, cache_obj_t *new_obj, cache_obj_t *old_obj) {
  if (old_obj->list_prev != NULL)
    old_obj->list_prev->list_next = new_obj;
  if (old_obj->list_next != NULL)
    old_obj->list_next->list_prev = new_obj;
  memcpy(new_obj, old_obj, sizeof(cache_obj_t));
  update_monitored_ptr(hashtable, new_obj, old_obj);
}

static inline void delete_obj_in_table(hashtable_t* hashtable, cache_obj_t* cache_obj){
  if (cache_obj->list_prev != NULL)
    cache_obj->list_prev->list_next = cache_obj->list_next;
  if (cache_obj->list_next != NULL)
    cache_obj->list_next->list_prev = cache_obj->list_prev;
  update_monitored_ptr(hashtable, NULL, cache_obj);
}

/**
 * move cache_obj from old_table into new_table
 * @param hashtable
 * @param cache_obj
 * @param part_of_old_table
 */
static inline void
_move_into_new_table(hashtable_t *hashtable, cache_obj_t *cache_obj, bool part_of_old_table) {
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id_int) & hashmask(hashtable->hash_power);
  if (OBJ_EMPTY(&(hashtable->table[hv]))) {
    _move_obj_to_new_loc(hashtable, &hashtable->table[hv], cache_obj);
    hashtable->table[hv].hash_next = NULL;
    if (!part_of_old_table)
      free_cache_obj(cache_obj);
  } else {
    if (part_of_old_table) {
      cache_obj_t *new_cache_obj = create_cache_obj_from_request(NULL);
      _move_obj_to_new_loc(hashtable, new_cache_obj, cache_obj);
      cache_obj = new_cache_obj;
    }
    _last_obj_in_bucket(hashtable, hv)->hash_next = cache_obj;
    cache_obj->hash_next = NULL;
  }
}


/************************ hashtable func ************************/
hashtable_t *create_chained_hashtable(const uint16_t hash_power) {
  hashtable_t *hashtable = my_malloc(hashtable_t);
  memset(hashtable, 0, sizeof(hashtable_t));
  hashtable->hash_power = hash_power;
  hashtable->table = my_malloc_n(cache_obj_t, hashsize(hashtable->hash_power));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table, sizeof(cache_obj_t)*hashsize(hashtable->hash_power), MADV_HUGEPAGE);
#endif
  if (hashtable->table == NULL) {
    ERROR("unable to allocate hash table (size %llu)\n",
          (unsigned long long) sizeof(cache_obj_t) * hashsize(hashtable->hash_power));
    abort();
  }
  memset(hashtable->table, 0, hashsize(hash_power) * sizeof(cache_obj_t));
  hashtable->hash_power = hash_power;
  hashtable->n_cur_item = 0;
  return hashtable;
}

cache_obj_t *chained_hashtable_find(hashtable_t *hashtable, request_t *req) {
  cache_obj_t *cache_obj, *ret = NULL;
  uint64_t hv = get_hash_value_int_64(&req->obj_id_int) & hashmask(hashtable->hash_power);
  cache_obj = &hashtable->table[hv];
  if (OBJ_EMPTY(cache_obj)) {
    // the object does not exist
    return NULL;
  }

  int depth = 0;
  while (cache_obj) {
    depth += 1;
    if (cache_obj->obj_id_int == req->obj_id_int) {
      ret = cache_obj;
      break;
    }
    cache_obj = cache_obj->hash_next;
  }
//  if (depth > 6){
//    printf("depth %d size %d %d\n", depth, hashtable->n_cur_item, hashsize(hashtable->hash_power));
//  }
  return ret;
}


cache_obj_t *chained_hashtable_insert(hashtable_t *hashtable, request_t *req) {
  if (hashtable->n_cur_item > (uint64_t) (hashsize(hashtable->hash_power) * CHAINED_HASHTABLE_EXPAND_THRESHOLD))
    _chained_hashtable_expand(hashtable);

  uint64_t hv = get_hash_value_int_64(&req->obj_id_int) & hashmask(hashtable->hash_power);
  cache_obj_t *cache_obj = &hashtable->table[hv];
  if (OBJ_EMPTY(cache_obj)) {
    // this place is available
    copy_request_to_cache_obj(cache_obj, req);
  } else {
    // this place is occupied, create a new cache_obj struct, linked to the end of chain
    cache_obj_t *new_cache_obj = create_cache_obj_from_request(req);
//    print_cache_obj(cache_obj);
//    print_cache_obj(new_cache_obj);
    while (cache_obj->hash_next) {
      cache_obj = cache_obj->hash_next;
    }
    cache_obj->hash_next = new_cache_obj;
    cache_obj = new_cache_obj;
  }
  hashtable->n_cur_item += 1;
  return cache_obj;
}

/* you need to free the extra_metadata before deleting from hash table */
void chained_hashtable_delete(hashtable_t *hashtable, cache_obj_t *cache_obj) {
  hashtable->n_cur_item -= 1;
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id_int) & hashmask(hashtable->hash_power);
  cache_obj_t *cache_obj_in_bucket = &hashtable->table[hv];
  assert(!OBJ_EMPTY(cache_obj_in_bucket));
  if (cache_obj == cache_obj_in_bucket) {
    if (cache_obj_in_bucket->hash_next) {
      cache_obj_t *old_obj = cache_obj_in_bucket->hash_next;
      cache_obj_t *new_obj = cache_obj_in_bucket;
      _move_obj_to_new_loc(hashtable, new_obj, old_obj);
      free_cache_obj(old_obj);
    } else {
      delete_obj_in_table(hashtable, cache_obj_in_bucket);
      memset(cache_obj_in_bucket, 0, sizeof(cache_obj_t));
    }
  } else {
    cache_obj_t *prev_cache_obj = NULL;
    while (cache_obj_in_bucket) {
      prev_cache_obj = cache_obj_in_bucket;
      cache_obj_in_bucket = cache_obj_in_bucket->hash_next;
      if (cache_obj_in_bucket == cache_obj) {
        prev_cache_obj->hash_next = cache_obj->hash_next;
        delete_obj_in_table(hashtable, cache_obj);
//        memset(cache_obj, 0, sizeof(cache_obj_t));
        free_cache_obj(cache_obj);
        break;
      }
    }
    assert(cache_obj_in_bucket != NULL);
  }
}


cache_obj_t *chained_hashtable_rand_obj(hashtable_t *hashtable) {
  uint64_t pos = next_rand() & hashmask(hashtable->hash_power);
  while (OBJ_EMPTY(&hashtable->table[pos])){
    pos = (pos + 1) & hashmask(hashtable->hash_power);
  }
  return &hashtable->table[pos];
}

void chained_hashtable_foreach(hashtable_t *hashtable, hashtable_iter iter_func, void *user_data) {
  cache_obj_t *cur_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hash_power); i++) {
    if (OBJ_EMPTY(&hashtable->table[i]))
      continue;

    cur_obj = &hashtable->table[i];
    iter_func(cur_obj, user_data);
    while (cur_obj->hash_next) {
      cur_obj = cur_obj->hash_next;
      iter_func(cur_obj, user_data);
    }
  }
}

void free_chained_hashtable(hashtable_t *hashtable) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hash_power); i++) {
    if (OBJ_EMPTY(&hashtable->table[i]))
      continue;

    cur_obj = hashtable->table[i].hash_next;
    while (cur_obj) {
      next_obj = cur_obj->hash_next;
      my_free(sizeof(cache_obj_t), cur_obj);
      cur_obj = next_obj;
    }
  }
  my_free(sizeof(cache_obj_t) * hashsize(hashtable->hash_power), hashtable->table);
  if (hashtable->n_allocated_ptrs > 0)
    my_free(sizeof(cache_obj_t **) * hashtable->n_allocated_ptrs, hashtable->monitored_ptrs);
  my_free(sizeof(hashtable_t), hashtable);
}


/* grows the hashtable to the next power of 2. */
void _chained_hashtable_expand(hashtable_t *hashtable) {
  cache_obj_t *old_table = hashtable->table;
  hashtable->table = my_malloc_n(cache_obj_t, hashsize(++hashtable->hash_power));
  memset(hashtable->table, 0, hashsize(hashtable->hash_power) * sizeof(cache_obj_t));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table, sizeof(cache_obj_t)*hashsize(hashtable->hash_power), MADV_HUGEPAGE);
#endif
  ASSERT_NOT_NULL(hashtable->table, "unable to grow hashtable to size %llu\n", hashsizeULL(hashtable->hash_power));

  // move from old table into new hash table
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize((hashtable->hash_power - 1)); i++) {
    if (OBJ_EMPTY(&old_table[i]))
      continue;

    cur_obj = &old_table[i];
    next_obj = cur_obj->hash_next;
    _move_into_new_table(hashtable, cur_obj, true);
    while (next_obj) {
      cur_obj = next_obj;
      next_obj = cur_obj->hash_next;
      _move_into_new_table(hashtable, cur_obj, false);
    }
  }
  my_free(sizeof(cache_obj_t) * hashsize(hashtable->hash_power-1), old_table);
  VERBOSE("hashtable resized from %llu to %llu\n", hashsizeULL((uint16_t) (hashtable->hash_power - 1)),
        hashsizeULL(hashtable->hash_power));
//  check_chained_hashtable_integrity(hashtable);
//  check_chained_hashtable_integrity2(hashtable, *hashtable->monitored_ptrs[0]);
}

/**
 * because in hashtableV1 some of the cache_obj are baked into hashtable and when their content are moved internally,
 * either due to hashtable expansion or delete, their memory address will change, so any externally pointers referencing
 * to these pointers will be updated, hashtable->monitored_ptrs are those pointers used externally
 *
 * this function adds an externally monitored pointer into monitoring
 * @param hashtable
 * @param cache_obj
 */
void chained_hashtable_add_ptr_to_monitoring(hashtable_t *hashtable, cache_obj_t **cache_obj) {
  if (hashtable->n_monitored_ptrs == hashtable->n_allocated_ptrs) {
    cache_obj_t ***old_ptrs = hashtable->monitored_ptrs;
    uint64_t old_ptr_size = sizeof(cache_obj_t **) * hashtable->n_allocated_ptrs;
    hashtable->n_allocated_ptrs = MAX(8, hashtable->n_allocated_ptrs * 2);
    hashtable->monitored_ptrs = my_malloc_n(cache_obj_t**, hashtable->n_allocated_ptrs);
    assert(hashtable->monitored_ptrs != NULL);
    memcpy(hashtable->monitored_ptrs, old_ptrs, sizeof(cache_obj_t **) * hashtable->n_monitored_ptrs);
    my_free(old_ptr_size, old_ptrs);
  }
  hashtable->monitored_ptrs[hashtable->n_monitored_ptrs++] = cache_obj;
}

void chained_hashtable_remove_ptr_from_monitoring(hashtable_t *hashtable, cache_obj_t *cache_obj) {
  ;
}


void check_chained_hashtable_integrity(hashtable_t *hashtable) {
  cache_obj_t *cur_obj;
  uint64_t hv;
  for (uint64_t i = 0; i < hashsize(hashtable->hash_power); i++) {
    cur_obj = &(hashtable->table[i]);
    while(cur_obj && !OBJ_EMPTY(cur_obj)){
      hv = get_hash_value_int_64(&cur_obj->obj_id_int) & hashmask(hashtable->hash_power);
      assert(i == hv);
      cur_obj = cur_obj->hash_next;
    }
  }
}

void check_chained_hashtable_integrity2(hashtable_t *hashtable, cache_obj_t *head) {
  THIS_IS_DEBUG_FUNC;

  assert(head->list_prev == NULL);
  cache_obj_t *cur_obj = head, *cur_obj_in_chain;
  uint64_t hv;
  uint32_t list_idx = 0;
  while (cur_obj) {
    hv = get_hash_value_int_64(&cur_obj->obj_id_int) & hashmask(hashtable->hash_power);
    cur_obj_in_chain = &hashtable->table[hv];
    while (cur_obj_in_chain && cur_obj != cur_obj_in_chain) {
      cur_obj_in_chain = cur_obj_in_chain->hash_next;
    }
    assert(cur_obj == cur_obj_in_chain);
    cur_obj = cur_obj->list_next;
    list_idx ++;
    assert(head != cur_obj);
  }
}

#ifdef __cplusplus
}
#endif
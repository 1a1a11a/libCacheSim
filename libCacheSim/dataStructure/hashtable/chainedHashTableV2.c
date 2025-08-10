//
// TODO
// This hash table stores pointers to cache_obj_t in the table, it uses
// one-level of indirection.
// draw a table
// |----------------|
// |     void*      | ----> cache_obj_t* ----> cache_obj_t* ----> NULL
// |----------------|
// |     void*      | ----> cache_obj_t*
// |----------------|
// |     void*      | ----> NULL
// |----------------|
// |     void*      | ----> cache_obj_t* ----> cache_obj_t* ----> nULL
// |----------------|
// |     void*      | ----> NULL
// |----------------|
// |     void*      | ----> NULL
// |----------------|
//
//

#ifdef __cplusplus
extern "C" {
#endif

#include "chainedHashTableV2.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../hash/hash.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/macro.h"
#include "utils/include/mymath.h"

#define OBJ_EMPTY(cache_obj) ((cache_obj)->obj_size == 0)
#define NEXT_OBJ(cur_obj) (((cache_obj_t *)(cur_obj))->hash_next)

static void _copy_entries(hashtable_t *new_table, cache_obj_t **old_table,
                          uint64_t old_size);
static void _chained_hashtable_shrink_v2(hashtable_t *hashtable);
static void _chained_hashtable_expand_v2(hashtable_t *hashtable);
static void print_hashbucket_item_distribution(const hashtable_t *hashtable);

/************************ helper func ************************/
/**
 * get the last object in the hash bucket
 */
static inline cache_obj_t *_last_obj_in_bucket(const hashtable_t *hashtable,
                                               const uint64_t hv) {
  cache_obj_t *cur_obj_in_bucket = hashtable->ptr_table[hv];
  while (cur_obj_in_bucket->hash_next) {
    cur_obj_in_bucket = cur_obj_in_bucket->hash_next;
  }
  return cur_obj_in_bucket;
}

/* add an object to the hashtable */
static inline void add_to_table(hashtable_t *hashtable,
                                cache_obj_t *cache_obj) {
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id) &
                hashmask(hashtable->hashpower);
  if (hashtable->ptr_table[hv] == NULL) {
    hashtable->ptr_table[hv] = cache_obj;
    return;
  }
  cache_obj_t *head_ptr = hashtable->ptr_table[hv];

  cache_obj->hash_next = head_ptr;
  hashtable->ptr_table[hv] = cache_obj;

#ifdef HASHTABLE_DEBUG
  cache_obj_t *curr_obj = cache_obj->hash_next;
  while (curr_obj) {
    assert(curr_obj->obj_id != cache_obj->obj_id);
    curr_obj = curr_obj->hash_next;
  }
#endif
}

/* free object, called by other functions when iterating through the hashtable
 */
static inline void foreach_free_obj(cache_obj_t *cache_obj, void *user_data) {
  my_free(sizeof(cache_obj_t), cache_obj);
}

/************************ hashtable func ************************/
hashtable_t *create_chained_hashtable_v2(const uint16_t hashpower) {
  hashtable_t *hashtable = my_malloc(hashtable_t);
  memset(hashtable, 0, sizeof(hashtable_t));

  size_t size = sizeof(cache_obj_t *) * hashsize(hashtable->hashpower);
  hashtable->ptr_table = my_malloc_n(cache_obj_t *, hashsize(hashpower));
  if (hashtable->ptr_table == NULL) {
    ERROR("allocate hash table %zu entry * %lu B = %ld MiB failed\n",
          sizeof(cache_obj_t *), (unsigned long)(hashsize(hashpower)),
          (long)(sizeof(cache_obj_t *) * hashsize(hashpower) / 1024 / 1024));
    exit(1);
  }
  memset(hashtable->ptr_table, 0, size);

#ifdef USE_HUGEPAGE
  madvise(hashtable->table, size, MADV_HUGEPAGE);
#endif
  hashtable->external_obj = false;
  hashtable->hashpower = hashpower;
  hashtable->n_obj = 0;
  return hashtable;
}

cache_obj_t *chained_hashtable_find_obj_id_v2(const hashtable_t *hashtable,
                                              const obj_id_t obj_id) {
  cache_obj_t *cache_obj = NULL;
  uint64_t hv = get_hash_value_int_64(&obj_id);
  hv = hv & hashmask(hashtable->hashpower);
  cache_obj = hashtable->ptr_table[hv];

  while (cache_obj) {
    if (cache_obj->obj_id == obj_id) {
      return cache_obj;
    }
    cache_obj = cache_obj->hash_next;
  }
  return cache_obj;
}

cache_obj_t *chained_hashtable_find_v2(const hashtable_t *hashtable,
                                       const request_t *req) {
  return chained_hashtable_find_obj_id_v2(hashtable, req->obj_id);
}

cache_obj_t *chained_hashtable_find_obj_v2(const hashtable_t *hashtable,
                                           const cache_obj_t *obj_to_find) {
  return chained_hashtable_find_obj_id_v2(hashtable, obj_to_find->obj_id);
}

/* the user needs to make sure the added object is not in the hash table */
cache_obj_t *chained_hashtable_insert_v2(hashtable_t *hashtable,
                                         const request_t *req) {
  if (hashtable->n_obj > (uint64_t)(hashsize(hashtable->hashpower) *
                                    CHAINED_HASHTABLE_EXPAND_THRESHOLD)) {
    _chained_hashtable_expand_v2(hashtable);
  }

  cache_obj_t *new_cache_obj = create_cache_obj_from_request(req);
  add_to_table(hashtable, new_cache_obj);
  hashtable->n_obj += 1;
  return new_cache_obj;
}

/* the user needs to make sure the added object is not in the hash table */
cache_obj_t *chained_hashtable_insert_obj_v2(hashtable_t *hashtable,
                                             cache_obj_t *cache_obj) {
  DEBUG_ASSERT(hashtable->external_obj);
  if (hashtable->n_obj > (uint64_t)(hashsize(hashtable->hashpower) *
                                    CHAINED_HASHTABLE_EXPAND_THRESHOLD))
    _chained_hashtable_expand_v2(hashtable);

  add_to_table(hashtable, cache_obj);
  hashtable->n_obj += 1;
  return cache_obj;
}

/* you need to free the extra_metadata before deleting from hash table */
void chained_hashtable_delete_v2(hashtable_t *hashtable,
                                 cache_obj_t *cache_obj) {
  hashtable->n_obj -= 1;
  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id) &
                hashmask(hashtable->hashpower);
  if (hashtable->ptr_table[hv] == cache_obj) {
    hashtable->ptr_table[hv] = cache_obj->hash_next;
    if (!hashtable->external_obj) free_cache_obj(cache_obj);
    return;
  }

  static int max_chain_len = 64;
  int chain_len = 1;
  cache_obj_t *cur_obj = hashtable->ptr_table[hv];
  while (cur_obj != NULL && cur_obj->hash_next != cache_obj) {
    cur_obj = cur_obj->hash_next;
    chain_len += 1;
  }

  if (chain_len > max_chain_len) {
    max_chain_len = chain_len;
    WARN("hashtable remove %lu chain len %d, hashtable load %ld/%ld %lf\n",
         (unsigned long)cache_obj->obj_id, max_chain_len,
         (long)hashtable->n_obj, (long)hashsize(hashtable->hashpower),
         (double)hashtable->n_obj / hashsize(hashtable->hashpower));

    print_hashbucket_item_distribution(hashtable);
  }

  // the object to remove is not in the hash table
  DEBUG_ASSERT(cur_obj != NULL);
  cur_obj->hash_next = cache_obj->hash_next;
  if (!hashtable->external_obj) {
    free_cache_obj(cache_obj);
  }
}

bool chained_hashtable_try_delete_v2(hashtable_t *hashtable,
                                     cache_obj_t *cache_obj) {
  static int max_chain_len = 1;

  uint64_t hv = get_hash_value_int_64(&cache_obj->obj_id) &
                hashmask(hashtable->hashpower);
  if (hashtable->ptr_table[hv] == cache_obj) {
    hashtable->ptr_table[hv] = cache_obj->hash_next;
    hashtable->n_obj -= 1;
    if (!hashtable->external_obj) free_cache_obj(cache_obj);
    return true;
  }

  int chain_len = 1;
  cache_obj_t *cur_obj = hashtable->ptr_table[hv];
  while (cur_obj != NULL && cur_obj->hash_next != cache_obj) {
    cur_obj = cur_obj->hash_next;
    chain_len += 1;
  }

  if (chain_len > 16 && chain_len > max_chain_len) {
    max_chain_len = chain_len;
    //    WARN("hashtable remove %ld, hv %lu, max chain len %d, hashtable load
    //    %ld/%ld %lf\n",
    //           (long) cache_obj->obj_id,
    //           (unsigned long) hv, max_chain_len,
    //           (long) hashtable->n_obj,
    //           (long) hashsize(hashtable->hashpower),
    //           (double) hashtable->n_obj / hashsize(hashtable->hashpower)
    //    );

    //    cache_obj_t *tmp_obj = hashtable->ptr_table[hv];
    //    while (tmp_obj) {
    //      printf("%ld (%d), ", (long) tmp_obj->obj_id, tmp_obj->LSC.in_cache);
    //      tmp_obj = tmp_obj->hash_next;
    //    }
    //    printf("\n");
  }

  if (cur_obj != NULL) {
    cur_obj->hash_next = cache_obj->hash_next;
    hashtable->n_obj -= 1;
    if (!hashtable->external_obj) free_cache_obj(cache_obj);
    return true;
  }
  return false;
}

/**
 *  This function is used to delete an object from the hash table by object id.
 *  - if the object is in the hash table
 *      remove it
 *      return true.
 *  - if the object is not in the hash table
 *      return false.
 *
 *  @method chained_hashtable_delete_obj_id_v2
 *  @date   2023-11-28
 *  @param  hashtable                          [Handle to the hashtable]
 *  @param  obj_id                             [The object id to remove]
 *  @return                                    [true or false]
 */
bool chained_hashtable_delete_obj_id_v2(hashtable_t *hashtable,
                                        const obj_id_t obj_id) {
  uint64_t hv = get_hash_value_int_64(&obj_id) & hashmask(hashtable->hashpower);
  cache_obj_t *cur_obj = hashtable->ptr_table[hv];
  // the hash bucket is empty
  if (cur_obj == NULL) return false;

  // the object to remove is the first object in the hash bucket
  if (cur_obj->obj_id == obj_id) {
    hashtable->ptr_table[hv] = cur_obj->hash_next;
    if (!hashtable->external_obj) free_cache_obj(cur_obj);
    hashtable->n_obj -= 1;
    return true;
  }

  cache_obj_t *prev_obj;

  do {
    prev_obj = cur_obj;
    cur_obj = cur_obj->hash_next;
  } while (cur_obj != NULL && cur_obj->obj_id != obj_id);

  // the object to remove is in the hash bucket
  if (cur_obj != NULL) {
    prev_obj->hash_next = cur_obj->hash_next;
    if (!hashtable->external_obj) free_cache_obj(cur_obj);
    hashtable->n_obj -= 1;
    return true;
  }
  // the object to remove is not in the hash table
  return false;
}

cache_obj_t *chained_hashtable_rand_obj_v2(hashtable_t *hashtable) {
  uint64_t pos = next_rand() & hashmask(hashtable->hashpower);
  int n_tries = 0;
  while (hashtable->ptr_table[pos] == NULL) {
    n_tries += 1;
    if (n_tries > 32) {
      _chained_hashtable_shrink_v2(hashtable);
    }
    pos = next_rand() & hashmask(hashtable->hashpower);
  }

  int n_obj_in_bucket = 1;
  cache_obj_t *cur_obj = hashtable->ptr_table[pos];
  while (cur_obj->hash_next) {
    cur_obj = cur_obj->hash_next;
    n_obj_in_bucket += 1;
  }
  int rand_pos = next_rand() % n_obj_in_bucket;
  cur_obj = hashtable->ptr_table[pos];
  for (int i = 0; i < rand_pos; i++) {
    cur_obj = cur_obj->hash_next;
  }

  return cur_obj;
}

void chained_hashtable_foreach_v2(hashtable_t *hashtable,
                                  hashtable_iter iter_func, void *user_data) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hashpower); i++) {
    cur_obj = hashtable->ptr_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      iter_func(cur_obj, user_data);
      cur_obj = next_obj;
    }
  }
}

void free_chained_hashtable_v2(hashtable_t *hashtable) {
  if (!hashtable->external_obj)
    chained_hashtable_foreach_v2(hashtable, foreach_free_obj, NULL);
  my_free(sizeof(cache_obj_t *) * hashsize(hashtable->hashpower),
          hashtable->ptr_table);
  my_free(sizeof(hashtable_t), hashtable);
}

/**
 * @brief copy the entries from the old_table to the new_table
 *
 * @param new_table
 * @param old_table
 * @param old_size
 * @return * void
 */
static void _copy_entries(hashtable_t *new_table, cache_obj_t **old_table,
                          uint64_t old_size) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < old_size; i++) {
    cur_obj = old_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      cur_obj->hash_next = NULL;
      add_to_table(new_table, cur_obj);
      cur_obj = next_obj;
    }
  }
}

static void _chained_hashtable_shrink_v2(hashtable_t *hashtable) {
  cache_obj_t **old_table = hashtable->ptr_table;
  hashtable->ptr_table =
      my_malloc_n(cache_obj_t *, hashsize(--hashtable->hashpower));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table,
          sizeof(cache_obj_t *) * hashsize(hashtable->hashpower),
          MADV_HUGEPAGE);
#endif
  memset(hashtable->ptr_table, 0,
         hashsize(hashtable->hashpower) * sizeof(cache_obj_t *));
  ASSERT_NOT_NULL(hashtable->ptr_table,
                  "unable to shrink hashtable to size %llu\n",
                  hashsizeULL(hashtable->hashpower));

  DEBUG(
      "shrink hash table size from %llu to %llu, new hashtable load "
      "%llu/%llu\n",
      (unsigned long long)hashsizeULL((uint16_t)(hashtable->hashpower + 1)),
      (unsigned long long)hashsizeULL(hashtable->hashpower),
      (unsigned long long)hashtable->n_obj,
      (unsigned long long)hashsize(hashtable->hashpower));

  _copy_entries(hashtable, old_table, hashsize(hashtable->hashpower + 1));
  my_free(sizeof(cache_obj_t) * hashsize(hashtable->hashpower + 1), old_table);
}

/* grows the hashtable to the next power of 2. */
static void _chained_hashtable_expand_v2(hashtable_t *hashtable) {
  cache_obj_t **old_table = hashtable->ptr_table;
  hashtable->ptr_table =
      my_malloc_n(cache_obj_t *, hashsize(++hashtable->hashpower));
#ifdef USE_HUGEPAGE
  madvise(hashtable->table,
          sizeof(cache_obj_t *) * hashsize(hashtable->hashpower),
          MADV_HUGEPAGE);
#endif
  memset(hashtable->ptr_table, 0,
         hashsize(hashtable->hashpower) * sizeof(cache_obj_t *));
  ASSERT_NOT_NULL(hashtable->ptr_table,
                  "unable to grow hashtable to size %llu\n",
                  hashsizeULL(hashtable->hashpower));

  DEBUG(
      "expand hashtable from %llu to %llu entries, new hashtable load "
      "%llu/%llu\n",
      (unsigned long long)hashsizeULL((uint16_t)(hashtable->hashpower - 1)),
      (unsigned long long)hashsizeULL(hashtable->hashpower),
      (unsigned long long)hashtable->n_obj,
      (unsigned long long)hashsize(hashtable->hashpower));

  _copy_entries(hashtable, old_table, hashsize(hashtable->hashpower - 1));
  my_free(sizeof(cache_obj_t) * hashsize(hashtable->hashpower), old_table);
}

void check_hashtable_integrity_v2(const hashtable_t *hashtable) {
  cache_obj_t *cur_obj, *next_obj;
  for (uint64_t i = 0; i < hashsize(hashtable->hashpower); i++) {
    cur_obj = hashtable->ptr_table[i];
    while (cur_obj != NULL) {
      next_obj = cur_obj->hash_next;
      assert(i == (get_hash_value_int_64(&cur_obj->obj_id) &
                   hashmask(hashtable->hashpower)));
      cur_obj = next_obj;
    }
  }
}

static int count_n_obj_in_bucket(cache_obj_t *curr_obj) {
  obj_id_t obj_id_arr[64];
  int chain_len = 0;
  while (curr_obj != NULL) {
    obj_id_arr[chain_len] = curr_obj->obj_id;
    for (int i = 0; i < chain_len; i++) {
      if (obj_id_arr[i] == curr_obj->obj_id) {
        ERROR("obj_id %lu is duplicated in hashtable\n",
              (unsigned long)curr_obj->obj_id);
        abort();
      }
    }

    curr_obj = curr_obj->hash_next;
    chain_len += 1;
  }

  return chain_len;
}

static void print_hashbucket_item_distribution(const hashtable_t *hashtable) {
  int n_print = 0;
  int n_obj = 0;
  for (uint64_t i = 0; i < hashsize(hashtable->hashpower); i++) {
    int chain_len = count_n_obj_in_bucket(hashtable->ptr_table[i]);
    n_obj += chain_len;
    if (chain_len > 1) {
      printf("%d, ", chain_len);
      n_print++;
    }
    if (n_print == 20) {
      printf("\n");
      n_print = 0;
    }
  }
  printf("\n #################### %d \n", n_obj);
}

void print_chained_hashtable_v2(const hashtable_t *hashtable) {
  for (uint64_t i = 0; i < hashsize(hashtable->hashpower); i++) {
    cache_obj_t *cur_obj = hashtable->ptr_table[i];
    if (cur_obj == NULL) {
      continue;
    }
    printf("hash bucket %lu: ", (unsigned long)i);
    while (cur_obj != NULL) {
      printf("%lu, ", (unsigned long)cur_obj->obj_id);
      cur_obj = cur_obj->hash_next;
    }
    printf("\n");
  }
}

#ifdef __cplusplus
}
#endif

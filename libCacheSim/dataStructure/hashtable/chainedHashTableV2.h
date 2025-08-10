//
// Created by Juncheng Yang on 6/9/20.
//

#ifndef libCacheSim_CHAINEDHASHTABLEV2_H
#define libCacheSim_CHAINEDHASHTABLEV2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>

#include "hashtableStruct.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/request.h"

hashtable_t *create_chained_hashtable_v2(const uint16_t hashpower_init);

cache_obj_t *chained_hashtable_find_obj_id_v2(const hashtable_t *hashtable,
                                              const obj_id_t obj_id);

cache_obj_t *chained_hashtable_find_v2(const hashtable_t *hashtable,
                                       const request_t *req);

cache_obj_t *chained_hashtable_find_obj_v2(const hashtable_t *hashtable,
                                           const cache_obj_t *obj_to_evict);

/* return an empty cache_obj_t */
cache_obj_t *chained_hashtable_insert_v2(hashtable_t *hashtable,
                                         const request_t *req);

cache_obj_t *chained_hashtable_insert_obj_v2(hashtable_t *hashtable,
                                             cache_obj_t *cache_obj);

bool chained_hashtable_try_delete_v2(hashtable_t *hashtable,
                                     cache_obj_t *cache_obj);

void chained_hashtable_delete_v2(hashtable_t *hashtable,
                                 cache_obj_t *cache_obj);

cache_obj_t *chained_hashtable_rand_obj_v2(hashtable_t *hashtable);

void chained_hashtable_foreach_v2(hashtable_t *hashtable,
                                  hashtable_iter iter_func, void *user_data);

void print_chained_hashtable_v2(const hashtable_t *hashtable);

void free_chained_hashtable_v2(hashtable_t *hashtable);

void check_hashtable_integrity_v2(const hashtable_t *hashtable);

void check_hashtable_integrity2_v2(const hashtable_t *hashtable,
                                   const cache_obj_t *head);

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_CHAINEDHASHTABLEV2_H

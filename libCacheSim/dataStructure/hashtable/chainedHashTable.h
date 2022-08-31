//
// The code in chainedHashTable is modified based on the code from Memcached
//
//
// Created by Juncheng Yang on 6/7/20.
//

#ifndef libCacheSim_CHAINEDHASHTABLE_H
#define libCacheSim_CHAINEDHASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>

#include "../../include/libCacheSim/cacheObj.h"
#include "../../include/libCacheSim/request.h"
#include "hashtableStruct.h"

hashtable_t *create_chained_hashtable(const uint16_t hashpower_init);

cache_obj_t *chained_hashtable_find_obj(hashtable_t *hashtable,
                                        cache_obj_t *cache_obj);

cache_obj_t *chained_hashtable_find(hashtable_t *hashtable, obj_id_t obj_id);

cache_obj_t *chained_hashtable_find_req(hashtable_t *hashtable, request_t *req);

/* return an empty cache_obj_t */
cache_obj_t *chained_hashtable_insert(hashtable_t *hashtable, request_t *req);

void chained_hashtable_delete(hashtable_t *hashtable, cache_obj_t *cache_obj);

cache_obj_t *chained_hashtable_rand_obj(hashtable_t *hashtable);

void chained_hashtable_foreach(hashtable_t *hashtable, hashtable_iter iter_func,
                               void *user_data);

void free_chained_hashtable(hashtable_t *hashtable);

/**
 * because in hashtableV1 some of the cache_obj are baked into hashtable and
 * when their content are moved internally, either due to hashtable expansion or
 * delete, their memory address will change, so any externally pointers
 * referencing to these pointers will be updated, hashtable->monitored_ptrs are
 * those pointers used externally
 *
 * this function adds an externally monitored pointer into monitoring
 * @param hashtable
 * @param cache_obj
 */
void chained_hashtable_add_ptr_to_monitoring(hashtable_t *hashtable,
                                             cache_obj_t **ptr);

void chained_hashtable_count_chain_length(hashtable_t *hashtable);

void check_chained_hashtable_integrity(hashtable_t *hashtable);

void check_chained_hashtable_integrity2(hashtable_t *hashtable,
                                        cache_obj_t *head);

void _chained_hashtable_expand(hashtable_t *hashtable);

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_CHAINEDHASHTABLE_H

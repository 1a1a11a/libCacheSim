//
// Created by Juncheng Yang on 6/9/20.
//

#ifndef LIBMIMIRCACHE_CHAINEDHASHTABLEV2_H
#define LIBMIMIRCACHE_CHAINEDHASHTABLEV2_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <assert.h>
#include "../../include/mimircache/struct.h"
#include "../../include/mimircache/request.h"
#include "../../include/mimircache/cacheObj.h"
#include "hashtableStruct.h"


hashtable_t *create_chained_hashtable_v2(const uint16_t hashpower_init);

cache_obj_t *chained_hashtable_find_v2(hashtable_t *hashtable, request_t *req);

/* return an empty cache_obj_t */
cache_obj_t *chained_hashtable_insert_v2(hashtable_t *hashtable, request_t *req);

void chained_hashtable_delete_v2(hashtable_t *hashtable, cache_obj_t *cache_obj);

cache_obj_t *chained_hashtable_rand_obj_v2(hashtable_t *hashtable);

void chained_hashtable_foreach_v2(hashtable_t *hashtable, hashtable_iter iter_func, void *user_data);

void free_chained_hashtable_v2(hashtable_t *hashtable);

void check_hashtable_integrity_v2(hashtable_t *hashtable);

void check_hashtable_integrity2_v2(hashtable_t *hashtable, cache_obj_t *head);

#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CHAINEDHASHTABLEV2_H

//
// Created by Juncheng Yang on 6/1/20.
//

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "hashtableStruct.h"
#include "../../include/libCacheSim/cacheObj.h"
#include "../../include/libCacheSim/request.h"
#include "../../include/config.h"
#include "../../utils/include/mymath.h"


//#if HASHTABLE_TYPE == CHAINED_HASHTABLE
//#include "chainedHashTable.h"
//#elif HASHTABLE_TYPE == CUCKCOO_HASHTABLE
//#include "cuckooHashTable.h"
//#else
//#include "chainedHashTable.h"
//#endif


#if HASHTABLE_TYPE == CHAINED_HASHTABLE
#include "chainedHashTable.h"
#define create_hashtable(hashpower) create_chained_hashtable(hashpower)
#define hashtable_find(hashtable, obj_id) chained_hashtable_find(hashtable, obj_id)
#define hashtable_find_req(hashtable, req) chained_hashtable_find(hashtable, req)
#define hashtable_find_obj(hashtable, cache_obj) chained_hashtable_find_obj(hashtable, cache_obj)
#define hashtable_insert(hashtable, req) chained_hashtable_insert(hashtable, req)
#define hashtable_insert_obj(hashtable, cache_obj) assert(0);
#define hashtable_delete(hashtable, cache_obj) chained_hashtable_delete(hashtable, cache_obj)
#define hashtable_rand_obj(hashtable) chained_hashtable_rand_obj(hashtable)
#define hashtable_foreach(hashtable, iter_func, user_data) chained_hashtable_foreach(hashtable, iter_func, user_data)
#define free_hashtable(hashtable) free_chained_hashtable(hashtable)
#define hashtable_add_ptr_to_monitoring(hashtable, ptr) chained_hashtable_add_ptr_to_monitoring(hashtable, ptr)

#elif HASHTABLE_TYPE == CHAINED_HASHTABLEV2
#include "chainedHashTableV2.h"
#define create_hashtable(hashpower) create_chained_hashtable_v2(hashpower)
#define hashtable_find(hashtable, req) chained_hashtable_find_v2(hashtable, req)
#define hashtable_find_obj_id(hashtable, obj_id) chained_hashtable_find_obj_id_v2(hashtable, obj_id)
#define hashtable_find_obj(hashtable, cache_obj) chained_hashtable_find_obj_v2(hashtable, cache_obj)
#define hashtable_insert(hashtable, req) chained_hashtable_insert_v2(hashtable, req)
#define hashtable_insert_obj(hashtable, cache_obj) chained_hashtable_insert_obj_v2(hashtable, cache_obj)
#define hashtable_delete(hashtable, cache_obj) chained_hashtable_delete_v2(hashtable, cache_obj)
#define hashtable_delete_obj_id(hashtable, cache_obj) chained_hashtable_delete_obj_id_v2(hashtable, obj_id)
#define hashtable_rand_obj(hashtable) chained_hashtable_rand_obj_v2(hashtable)
#define hashtable_foreach(hashtable, iter_func, user_data) chained_hashtable_foreach_v2(hashtable, iter_func, user_data)
#define free_hashtable(hashtable) free_chained_hashtable_v2(hashtable)
#define hashtable_add_ptr_to_monitoring(hashtable, ptr)

#elif HASHTABLE_TYPE == CUCKCOO_HASHTABLE
#include "cuckooHashTable.h"
#error not implemented
#else
#error not implemented
#endif

#ifdef __cplusplus
}
#endif

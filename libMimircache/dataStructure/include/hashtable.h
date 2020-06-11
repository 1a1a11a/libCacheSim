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
#include "../../include/config.h"
#include "hashtableStruct.h"

#include "../../utils/include/mathUtils.h"


//#if HASHTABLE_TYPE == CHAINED_HASHTABLE
//#include "chainedHashTable.h"
//#elif HASHTABLE_TYPE == CUCKCOO_HASHTABLE
//#include "cuckooHashTable.h"
//#else
//#include "chainedHashTable.h"
//#endif


#if HASHTABLE_TYPE == CHAINED_HASHTABLE
#if HASHTABLE_VER == 1
#include "chainedHashTable.h"
#define create_hashtable(hash_power) create_chained_hashtable(hash_power)
#define hashtable_find(hashtable, req) chained_hashtable_find(hashtable, req)
#define hashtable_insert(hashtable, req) chained_hashtable_insert(hashtable, req)
#define hashtable_delete(hashtable, cache_obj) chained_hashtable_delete(hashtable, cache_obj)
#define hashtable_rand_obj(hashtable) chained_hashtable_rand_obj(hashtable)
#define hashtable_foreach(hashtable, iter_func, user_data) chained_hashtable_foreach(hashtable, iter_func, user_data)
#define free_hashtable(hashtable) free_chained_hashtable(hashtable)
#define hashtable_add_ptr_to_monitoring(hashtable, ptr) chained_hashtable_add_ptr_to_monitoring(hashtable, ptr)

#elif HASHTABLE_VER == 2
#include "chainedHashTableV2.h"
#define create_hashtable(hash_power) create_chained_hashtable_v2(hash_power)
#define hashtable_find(hashtable, req) chained_hashtable_find_v2(hashtable, req)
#define hashtable_insert(hashtable, req) chained_hashtable_insert_v2(hashtable, req)
#define hashtable_delete(hashtable, cache_obj) chained_hashtable_delete_v2(hashtable, cache_obj)
#define hashtable_rand_obj(hashtable) chained_hashtable_rand_obj_v2(hashtable)
#define hashtable_foreach(hashtable, iter_func, user_data) chained_hashtable_foreach_v2(hashtable, iter_func, user_data)
#define free_hashtable(hashtable) free_chained_hashtable_v2(hashtable)
#define hashtable_add_ptr_to_monitoring(hashtable, ptr)
#endif

#elif HASHTABLE_TYPE == CUCKCOO_HASHTABLE
#include "cuckooHashTable.h"
#error not implemented
#else
#error not implemented
#endif

#ifdef __cplusplus
}
#endif
#endif //LIBMIMIRCACHE_HASHTABLE_H

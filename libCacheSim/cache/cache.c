//
// Created by Juncheng Yang on 6/20/20.
//

#include "../include/libCacheSim/cache.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <dlfcn.h>

/** this file contains both base function, which should be called by all eviction algorithms,
 *  and the queue related functions, which should be called by algorithm that uses only one queue and 
 *  needs to update the queue such as LRU and FIFO 
 **/

/**
 * @brief this function is called by all eviction algorithms to initialize the cache 
 * 
 * @param ccache_params common cache parameters
 * @param init_params eviction algorithm specific parameters
 * @return cache_t* pointer to the cache 
 */
cache_t *cache_struct_init(const char *const cache_name, common_cache_params_t params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->cache_name, cache_name, 31);
  cache->cache_size = params.cache_size;
  cache->eviction_params = NULL;
  cache->default_ttl = params.default_ttl;
  cache->per_obj_overhead = params.per_obj_overhead;
  cache->n_req = 0;
  cache->stat.cache_size = cache->cache_size;

  int hash_power = HASH_POWER_DEFAULT;
  if (params.hashpower > 0 && params.hashpower < 40) hash_power = params.hashpower;
  cache->hashtable = create_hashtable(hash_power);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->q_head);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->q_tail);

  return cache;
}

/**
 * @brief this function is called by all eviction algorithms to free the cache 
 * 
 * @param cache 
 */
void cache_struct_free(cache_t *cache) {
  free_hashtable(cache->hashtable);
  my_free(sizeof(cache_t), cache);
}

/**
 * @brief this function is called by all eviction algorithms to clone old cache with new size
 * 
 * @param old_cache
 * @param new_size
 * @return cache_t* pointer to the new cache
 */
cache_t *create_cache_with_new_size(cache_t *old_cache, uint64_t new_size) {
  common_cache_params_t cc_params = {.cache_size = new_size,
                                     .hashpower = old_cache->hashtable->hashpower,
                                     .default_ttl = old_cache->default_ttl,
                                     .per_obj_overhead = old_cache->per_obj_overhead};
  assert(sizeof(cc_params) == 24);
  cache_t *cache = old_cache->cache_init(cc_params, old_cache->init_params);
  return cache;
}

/**
 * @brief this function is called by all eviction algorithms to 
 * check whether an object is in the cache          
 * 
 * @param cache
 * @param req
 * @param update_cache whether to update the cache, 
 *  if true, the number of requests increases by 1, 
 *  the object size will be updated, 
 *  and if the object is expired, it is removed from the cache
 * @return cache_ck_hit, cache_ck_miss, cache_ck_expired 
 */
cache_ck_res_e cache_check_base(cache_t *cache, request_t *req, bool update_cache,
                                cache_obj_t **cache_obj_ret) {
  cache_ck_res_e ret = cache_ck_hit;
  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  if (cache_obj == NULL) {
    ret = cache_ck_miss;
  } else {
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
    if (cache_obj->exp_time != 0 && cache_obj->exp_time < req->real_time) {
      ret = cache_ck_expired;
    }
#endif
  }

  if (cache_obj_ret != NULL) {
    if (ret == cache_ck_hit) {
      *cache_obj_ret = cache_obj;
    } else
      *cache_obj_ret = NULL;
  }

  if (likely(update_cache)) {
    cache->n_req += 1;

    if (ret == cache_ck_hit) {
      if (unlikely(cache_obj->obj_size != req->obj_size)) {
        VVERBOSE("object size change from %u to %u\n", cache_obj->obj_size, req->obj_size);
        cache->occupied_size -= cache_obj->obj_size;
        cache->occupied_size += req->obj_size;
        cache_obj->obj_size = req->obj_size;
      }
    } else if (ret == cache_ck_expired) {
      cache->remove(cache, cache_obj->obj_id);
    }
  }

  return ret;
}

/**
 * @brief this function is called by all eviction algorithms to 
 * check whether an object is in the cache, 
 * if not, it will insert the object into the cache
 * basically, this is check -> return if hit, insert if miss -> evict if needed 
 * 
 * @param cache
 * @param req
 * @return cache_ck_hit, cache_ck_miss, cache_ck_expired 
 */
cache_ck_res_e cache_get_base(cache_t *cache, request_t *req) {
  VVVERBOSE("req %" PRIu64 ", obj %" PRIu64 ", obj_size %" PRIu32 ", cache size %" PRIu64
            "/%" PRIu64 "\n",
            cache->n_req, req->obj_id, req->obj_size, cache->occupied_size,
            cache->cache_size);

  cache_ck_res_e cache_check = cache->check(cache, req, true);

  bool admit = true;

#if defined(SUPPORT_ADMISSION) && SUPPORT_ADMISSION == 1
  if (cache->admit != NULL && !cache->admit(cache, req)) {
      admit = false;
  }
#endif
  if (admit) {
    if (req->obj_size + cache->per_obj_overhead <= cache->cache_size) {
      if (cache_check == cache_ck_miss) {
        while (cache->occupied_size + req->obj_size + cache->per_obj_overhead > cache->cache_size)
          cache->evict(cache, req, NULL);

        cache->insert(cache, req);
      }
    } else {
      static __thread bool has_printed = false;
      if (!has_printed) {
        has_printed = true; 
        WARN("req %" PRIu64 ": obj size %" PRIu32 " larger than cache size %" PRIu64 "\n",
                req->obj_id, req->obj_size, cache->cache_size);
      }
    }
  }
  return cache_check;
}

/**
 * @brief this function is called by all eviction algorithms to 
 * insert an object into the cache, update thee cache metadata
 * 
 * @param cache
 * @param req
 */
cache_obj_t *cache_insert_base(cache_t *cache, request_t *req) {
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (cache->default_ttl != 0 && req->ttl == 0) {
    req->ttl = (int32_t) cache->default_ttl;
  }
#endif
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
  return cache_obj;
}

/**
 * @brief this function is called by eviction algorithms that use 
 * a single queue to order objects, such as LRU, FIFO, etc. 
 * it inserts an object to the tail of queue
 * 
 * @param cache
 * @param req
 */
cache_obj_t *cache_insert_LRU(cache_t *cache, request_t *req) {
  cache_obj_t *cache_obj = cache_insert_base(cache, req);

  if (unlikely(cache->q_head == NULL)) {
    // an empty list, this is the first insert
    cache->q_head = cache_obj;
    cache->q_tail = cache_obj;
  } else {
    cache->q_tail->queue.next = cache_obj;
    cache_obj->queue.prev = cache->q_tail;
  }
  cache->q_tail = cache_obj;
  return cache_obj;
}

/**
 * @brief this function is called by all eviction algorithms that 
 * need to remove an object from the cache, it updates the cache metadata. 
 * 
 * @param cache
 * @param req
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj) {
  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);
  cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;
  hashtable_delete(cache->hashtable, obj);
}

void cache_evict_LRU(cache_t *cache,
                     __attribute__((unused)) request_t *req,
                     cache_obj_t *evicted_obj) {
  cache_obj_t *obj_to_evict = cache->q_head;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->q_head != NULL);
  DEBUG_ASSERT(cache->q_head != cache->q_head->queue.next);
  cache->q_head = cache->q_head->queue.next;
  if (likely(cache->q_head != NULL))
    cache->q_head->queue.prev = NULL;

  cache_remove_obj_base(cache, obj_to_evict);
  DEBUG_ASSERT(cache->q_head == NULL ||
                      cache->q_head != cache->q_head->queue.next);
  /** obj_to_evict is not freed or returned to hashtable, if you have
 * extra_metadata allocated with obj_to_evict, you need to free them now,
 * otherwise, there will be memory leakage **/
}

/**
 * @brief this function is a helper when we need the actual object
 * 
 * @param cache
 * @param req
 * @return cache_obj_t* 
 */
cache_obj_t *cache_get_obj(cache_t *cache, request_t *req) {
  return hashtable_find(cache->hashtable, req);
}

/**
 * @brief this function is a helper when we need the actual object
 * 
 * @param cache
 * @param obj_id_t
 */
cache_obj_t *cache_get_obj_by_id(cache_t *cache, obj_id_t id) {
  return hashtable_find_obj_id(cache->hashtable, id);
}

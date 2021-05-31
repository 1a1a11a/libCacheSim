//
// Created by Juncheng Yang on 6/20/20.
//

#include <dlfcn.h>
#include "../include/libCacheSim/cache.h"
#include "../dataStructure/hashtable/hashtable.h"


cache_t *cache_struct_init(const char *const cache_name,
                           common_cache_params_t params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->cache_name, cache_name, 31);
  cache->cache_size = params.cache_size;
  cache->eviction_params = NULL;
  cache->default_ttl = params.default_ttl;
  cache->per_obj_overhead = params.per_obj_overhead;
  cache->stat.cache_size = cache->cache_size;

  int hash_power = HASH_POWER_DEFAULT;
  if (params.hashpower > 0 && params.hashpower < 40)
    hash_power = params.hashpower;
  cache->hashtable = create_hashtable(hash_power);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->list_head);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->list_tail);

  return cache;
}

void cache_struct_free(cache_t *cache) {
  free_hashtable(cache->hashtable);
  my_free(sizeof(cache_t), cache);
}

cache_t *create_cache_with_new_size(cache_t *old_cache, uint64_t new_size) {
  common_cache_params_t cc_params = {.cache_size = new_size,
                                     .hashpower = old_cache->hashtable->hashpower,
                                     .default_ttl = old_cache->default_ttl,
                                     .per_obj_overhead = old_cache->per_obj_overhead};
  assert(sizeof(cc_params) == 24);
  cache_t *cache = old_cache->cache_init(cc_params, old_cache->init_params);
  return cache;
}

cache_ck_res_e cache_check_base(cache_t *cache, request_t *req, bool update_cache,
                           cache_obj_t **cache_obj_ret) {
  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);
  if (cache_obj_ret != NULL)
    *cache_obj_ret = cache_obj;
  if (cache_obj == NULL) {
    return cache_ck_miss;
  }

  cache_ck_res_e ret = cache_ck_hit;
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (cache->default_ttl != 0) {
    if (cache_obj->exp_time < req->real_time) {
      ret = cache_ck_expired;
      if (likely(update_cache))
        cache_obj->exp_time =
            req->real_time + (req->ttl != 0 ? req->ttl : cache->default_ttl);
    }
  }
#endif
  if (likely(update_cache)) {
    if (unlikely(cache_obj->obj_size != req->obj_size)) {
      cache->occupied_size -= cache_obj->obj_size;
      cache->occupied_size += req->obj_size;
      cache_obj->obj_size = req->obj_size;
    }
  }
  return ret;
}

cache_ck_res_e cache_get_base(cache_t *cache, request_t *req) {
  VVVERBOSE("req %" PRIu64 ", obj %" PRIu64 ", obj_size %" PRIu32
            ", cache size %" PRIu64 "/%" PRIu64 "\n",
            cache->req_cnt, req->obj_id, req->obj_size,
            cache->occupied_size, cache->cache_size);

  cache_ck_res_e cache_check = cache->check(cache, req, true);
  if (req->obj_size <= cache->cache_size) {
    if (cache_check == cache_ck_miss) {
      while (cache->occupied_size + req->obj_size + cache->per_obj_overhead > cache->cache_size)
        cache->evict(cache, req, NULL);

      cache->insert(cache, req);
    }
  } else {
    WARNING("req %"PRIu64 ": obj size %"PRIu32 " larger than cache size %"PRIu64 "\n",
            req->obj_id, req->obj_size, cache->cache_size);
  }
  return cache_check;
}

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

cache_obj_t *cache_insert_LRU(cache_t *cache, request_t *req) {
  cache_obj_t *cache_obj = cache_insert_base(cache, req);

  if (unlikely(cache->list_head == NULL)) {
    // an empty list, this is the first insert
    cache->list_head = cache_obj;
    cache->list_tail = cache_obj;
  } else {
    cache->list_tail->common.list_next = cache_obj;
    cache_obj->common.list_prev = cache->list_tail;
  }
  cache->list_tail = cache_obj;
  return cache_obj;
}

void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj) {
  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);
  cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;
  hashtable_delete(cache->hashtable, obj);
}

void cache_evict_LRU(cache_t *cache,
                     __attribute__((unused)) request_t *req,
                     cache_obj_t *evicted_obj) {
  // currently not handle the case when all objects are evicted

  cache_obj_t *obj_to_evict = cache->list_head;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->list_head != cache->list_head->common.list_next);
  cache->list_head = cache->list_head->common.list_next;
  cache->list_head->common.list_prev = NULL;

  cache_remove_obj_base(cache, obj_to_evict);
  DEBUG_ASSERT(cache->list_head != cache->list_head->common.list_next);
  /** obj_to_evict is not freed or returned to hashtable, if you have
 * extra_metadata allocated with obj_to_evict, you need to free them now,
 * otherwise, there will be memory leakage **/
}

cache_obj_t *cache_get_obj(cache_t *cache, request_t *req) {
  return hashtable_find(cache->hashtable, req);
}

cache_obj_t *cache_get_obj_by_id(cache_t *cache, obj_id_t id) {
  return hashtable_find_obj_id(cache->hashtable, id);
}


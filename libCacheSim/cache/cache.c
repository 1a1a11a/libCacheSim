//
// Created by Juncheng Yang on 6/20/20.
//

#include <dlfcn.h>
#include "../include/libCacheSim/cache.h"
#include "../dataStructure/hashtable/hashtable.h"

static void *_get_func_handle(char *func_name, const char *const cache_name,
                              bool must_have, bool internal_func) {
  static void *handle = NULL;
  if (handle == NULL) {
    handle = dlopen(NULL, RTLD_GLOBAL);
//    /* should not check err here, otherwise ubuntu will report err even though
//     * everything is OK
//    char *err = dlerror();
//    if (err != NULL){
//      ERROR("error dlopen main program %s\n", err);
//      abort();
//    }
//     */
  }

  char full_func_name[128];
  if (internal_func)
    sprintf(full_func_name, "_%s_%s", cache_name, func_name);
  else
    sprintf(full_func_name, "%s_%s", cache_name, func_name);

  void *func_ptr = dlsym(handle, full_func_name);

  if (must_have && func_ptr == NULL) {
    ERROR("unable to find %s error %s\n", full_func_name, dlerror());
    abort();
  }
  return func_ptr;
}

cache_t *cache_struct_init(const char *const cache_name,
                           common_cache_params_t params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->cache_name, cache_name, 31);
  cache->cache_size = params.cache_size;
  cache->cache_params = NULL;
  cache->default_ttl = params.default_ttl;
  cache->per_obj_overhead = params.per_obj_overhead;

  int hash_power = HASH_POWER_DEFAULT;
  if (params.hashpower > 0 && params.hashpower < 40)
    hash_power = params.hashpower;
  cache->hashtable = create_hashtable(hash_power);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->list_head);
  hashtable_add_ptr_to_monitoring(cache->hashtable, &cache->list_tail);

  cache->cache_init =
      (cache_init_func_ptr) _get_func_handle("init", cache_name, true, false);
  cache->cache_free =
      (cache_free_func_ptr) _get_func_handle("free", cache_name, true, false);
  cache->get =
      (cache_get_func_ptr) _get_func_handle("get", cache_name, true, false);
  cache->check =
      (cache_check_func_ptr) _get_func_handle("check", cache_name, true, false);
  cache->insert =
      (cache_insert_func_ptr) _get_func_handle("insert", cache_name, true, false);
  cache->evict =
      (cache_evict_func_ptr) _get_func_handle("evict", cache_name, true, false);
//  cache->remove_obj = (cache_remove_obj_func_ptr) _get_func_handle(
//      "remove_obj", cache_name, false, false);
  cache->remove = (cache_remove_func_ptr) _get_func_handle("remove", cache_name, false, false);

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

cache_ck_res_e cache_check(cache_t *cache, request_t *req, bool update_cache,
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

cache_ck_res_e cache_get(cache_t *cache, request_t *req) {
  VVVERBOSE("req %" PRIu64 ", obj %" PRIu64 ", obj_size %" PRIu32
            ", cache size %" PRIu64 "/%" PRIu64 "\n",
            cache->req_cnt, req->obj_id_int, req->obj_size,
            cache->occupied_size, cache->cache_size);

  cache_ck_res_e cache_check = cache->check(cache, req, true);
  if (req->obj_size <= cache->cache_size) {
    if (cache_check == cache_ck_miss) {
      while (cache->occupied_size + req->obj_size + cache->per_obj_overhead > cache->cache_size)
        cache->evict(cache, req, NULL);
      cache->insert(cache, req);
    }
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n",
            (long long) cache->req_cnt, (long) req->obj_size,
            (long) cache->cache_size);
  }
  cache->req_cnt += 1;
  return cache_check;
}

cache_obj_t *cache_insert_LRU(cache_t *cache, request_t *req) {
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (cache->default_ttl != 0 && req->ttl == 0) {
    req->ttl = cache->default_ttl;
  }
#endif
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;

  if (unlikely(cache->list_head == NULL)) {
    // an empty list, this is the first insert
    cache->list_head = cache_obj;
    cache->list_tail = cache_obj;
  } else {
    cache->list_tail->list_next = cache_obj;
    cache_obj->list_prev = cache->list_tail;
  }
  cache->list_tail = cache_obj;
  return cache_obj;
}

void cache_evict_LRU(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  // currently not handle the case when all objects are evicted

  cache_obj_t *obj_to_evict = cache->list_head;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
  cache->list_head = cache->list_head->list_next;
  cache->list_head->list_prev = NULL;

  DEBUG_ASSERT(cache->occupied_size >= obj_to_evict->obj_size);
  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  hashtable_delete(cache->hashtable, obj_to_evict);
  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
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

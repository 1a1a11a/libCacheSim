//
// Created by Juncheng Yang on 6/20/20.
//

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/cache.h"

/** this file contains both base function, which should be called by all
 *eviction algorithms, and the queue related functions, which should be called
 *by algorithm that uses only one queue and needs to update the queue such as
 *LRU and FIFO
 **/

/**
 * @brief this function is called by all eviction algorithms to initialize the
 * cache
 *
 * @param ccache_params common cache parameters
 * @param init_params eviction algorithm specific parameters
 * @return cache_t* pointer to the cache
 */
cache_t *cache_struct_init(const char *const cache_name,
                           const common_cache_params_t params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->cache_name, cache_name, 31);
  cache->cache_size = params.cache_size;
  cache->eviction_params = NULL;
  cache->admissioner = NULL;
  cache->future_stack_dist = NULL;
  cache->future_stack_dist_array_size = 0;
  cache->default_ttl = params.default_ttl;
  cache->n_req = 0;
  cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;

  int hash_power = HASH_POWER_DEFAULT;
  if (params.hashpower > 0 && params.hashpower < 40)
    hash_power = params.hashpower;
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
  if (cache->admissioner != NULL) cache->admissioner->free(cache->admissioner);
  my_free(sizeof(cache_t), cache);
}

/**
 * @brief this function is called by all eviction algorithms to clone old cache
 * with new size
 *
 * @param old_cache
 * @param new_size
 * @return cache_t* pointer to the new cache
 */
cache_t *create_cache_with_new_size(const cache_t *old_cache,
                                    uint64_t new_size) {
  common_cache_params_t cc_params = {
      .cache_size = new_size,
      .hashpower = old_cache->hashtable->hashpower,
      .default_ttl = old_cache->default_ttl,
      .consider_obj_metadata =
          old_cache->per_obj_metadata_size == 0 ? false : true,
  };
  assert(sizeof(cc_params) == 24);
  cache_t *cache = old_cache->cache_init(cc_params, old_cache->init_params);
  cache->future_stack_dist = old_cache->future_stack_dist;
  cache->future_stack_dist_array_size = old_cache->future_stack_dist_array_size;
  return cache;
}

/**
 * @brief whether the request can be inserted into cache
 *
 * @param cache
 * @param req
 * @return true
 * @return false
 */
bool cache_can_insert_default(cache_t *cache, const request_t *req) {
  if (cache->admissioner != NULL) {
    admissioner_t *admissioner = cache->admissioner;
    if (admissioner->admit(admissioner, req) == false) {
      DEBUG_ONCE(
          "admission algorithm does not admit: req %ld, obj %lu, size %lu\n",
          cache->n_req, (unsigned long)req->obj_id,
          (unsigned long)req->obj_size);
      return false;
    }
  }

  if (req->obj_size + cache->per_obj_metadata_size > cache->cache_size) {
    WARN_ONCE("%ld req, obj %lu, size %lu larger than cache size %lu\n",
              cache->n_req, (unsigned long)req->obj_id,
              (unsigned long)req->obj_size, (unsigned long)cache->cache_size);
    return false;
  }

  return true;
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
cache_ck_res_e cache_check_base(cache_t *cache, const request_t *req,
                                const bool update_cache,
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
    if (ret == cache_ck_hit) {
      if (unlikely(cache_obj->obj_size != req->obj_size)) {
        VVERBOSE("object size change from %u to %u\n", cache_obj->obj_size,
                 req->obj_size);
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
cache_ck_res_e cache_get_base(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  VVERBOSE("******* req %" PRIu64 ", obj %" PRIu64 ", obj_size %" PRIu32
           ", cache size %" PRIu64 "/%" PRIu64 "\n",
           cache->n_req, req->obj_id, req->obj_size, cache->occupied_size,
           cache->cache_size);

  cache_ck_res_e cache_check = cache->check(cache, req, true);

  if (cache_check == cache_ck_hit) {
    VVERBOSE("req %" PRIu64 ", obj %" PRIu64 " --- cache hit\n", cache->n_req,
             req->obj_id);
    return cache_check;
  }

  if (cache->can_insert(cache, req) == false) {
    return cache_check;
  }

  if (cache_check == cache_ck_miss) {
    while (cache->occupied_size + req->obj_size + cache->per_obj_metadata_size >
           cache->cache_size)
      cache->evict(cache, req, NULL);

    cache->insert(cache, req);
  }

  return cache_check;
}

/**
 * @brief this function is called by all caches to
 * insert an object into the cache, update the hash table and cache metadata
 *
 * @param cache
 * @param req
 */
cache_obj_t *cache_insert_base(cache_t *cache, const request_t *req) {
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache->occupied_size += cache_obj->obj_size + cache->per_obj_metadata_size;
  cache->n_obj += 1;

#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (cache->default_ttl != 0 && req->ttl == 0) {
    cache_obj->ttl = (int32_t)cache->default_ttl;
  }
#endif

#ifdef TRACK_EVICTION_R_AGE
  cache_obj->create_time = (int64_t)req->real_time;
#endif
#ifdef TRACK_EVICTION_V_AGE
  cache_obj->create_time = (int64_t)cache->n_req;
#endif

  return cache_obj;
}

/**
 * @brief this function is called by caches that use
 * a single queue to order objects, such as LRU, FIFO, etc.
 * it inserts an object to the head of queue
 *
 * @param cache
 * @param req
 */
cache_obj_t *cache_insert_LRU(cache_t *cache, const request_t *req) {
  cache_obj_t *cache_obj = cache_insert_base(cache, req);

  if (unlikely(cache->q_head == NULL)) {
    // an empty list, this is the first insert
    cache->q_head = cache_obj;
    cache->q_tail = cache_obj;
  } else {
    cache->q_head->queue.prev = cache_obj;
    cache_obj->queue.next = cache->q_head;
  }
  cache->q_head = cache_obj;
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
  DEBUG_ASSERT(cache->occupied_size >=
               obj->obj_size + cache->per_obj_metadata_size);
  cache->occupied_size -= (obj->obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;
  hashtable_delete(cache->hashtable, obj);
}

/**
 * @brief eviction an object using LRU
 *
 * @param cache
 * @param evicted_obj
 */
void cache_evict_LRU(cache_t *cache,
                     __attribute__((unused)) const request_t *req,
                     cache_obj_t *evicted_obj) {
  cache_obj_t *obj_to_evict = cache->q_tail;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->q_tail != NULL);
  DEBUG_ASSERT(cache->q_tail != cache->q_tail->queue.prev);

#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(cache, req->real_time - obj_to_evict->create_time);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(cache, cache->n_req - obj_to_evict->create_time);
#endif

  cache->q_tail = cache->q_tail->queue.prev;
  if (likely(cache->q_tail != NULL)) {
    cache->q_tail->queue.next = NULL;
  } else {
    /* cache->n_obj has not been updated */
    DEBUG_ASSERT(cache->n_obj == 1);
    cache->q_head = NULL;
  }

  cache_remove_obj_base(cache, obj_to_evict);
  DEBUG_ASSERT(cache->q_tail == NULL ||
               cache->q_tail != cache->q_tail->queue.prev);
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
cache_obj_t *cache_get_obj(cache_t *cache, const request_t *req) {
  return hashtable_find(cache->hashtable, req);
}

/**
 * @brief this function is a helper when we need the actual object
 *
 * @param cache
 * @param obj_id_t
 */
cache_obj_t *cache_get_obj_by_id(cache_t *cache, const obj_id_t id) {
  return hashtable_find_obj_id(cache->hashtable, id);
}

/**
 * @brief print the recorded eviction age
 *
 * @param cache
 */
void print_log2_eviction_age(const cache_t *cache) {
  printf("eviction age %d:%ld, ", 1, (long)cache->log_eviction_age_cnt[0]);
  for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->log_eviction_age_cnt[i] > 1000000)
      printf("%lu:%.1lfm, ", 1lu << i,
             (double)cache->log_eviction_age_cnt[i] / 1000000.0);
    else if (cache->log_eviction_age_cnt[i] > 1000)
      printf("%lu:%.1lfk, ", 1lu << i,
             (double)cache->log_eviction_age_cnt[i] / 1000.0);
    else if (cache->log_eviction_age_cnt[i] > 0)
      printf("%lu:%ld, ", 1lu << i, (long)cache->log_eviction_age_cnt[i]);
  }
  printf("\n");
}

/**
 * @brief print the recorded eviction age
 *
 * @param cache
 */
void print_eviction_age(const cache_t *cache) {
  printf("eviction age %d:%ld, ", 1, (long)cache->log_eviction_age_cnt[0]);
  for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->log_eviction_age_cnt[i] > 1000000)
      printf("%lld:%.1lfm, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)),
             (double)cache->log_eviction_age_cnt[i] / 1000000.0);
    else if (cache->log_eviction_age_cnt[i] > 1000)
      printf("%lld:%.1lfk, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)),
             (double)cache->log_eviction_age_cnt[i] / 1000.0);
    else if (cache->log_eviction_age_cnt[i] > 0)
      printf("%lld:%ld, ", (long long)(pow(EVICTION_AGE_LOG_BASE, i)),
             (long)cache->log_eviction_age_cnt[i]);
  }
  printf("\n");
}

/**
 * @brief dump the eviction age distribution to a file
 *
 * @param cache
 * @param ofilepath
 * @return true
 * @return false
 */
bool dump_log2_eviction_age(const cache_t *cache, const char *ofilepath) {
  FILE *ofile = fopen(ofilepath, "a");
  if (ofile == NULL) {
    perror("fopen failed");
    return false;
  }

  fprintf(ofile, "%s, cache size: %lu, ", cache->cache_name,
          (unsigned long)cache->cache_size);
  fprintf(ofile, "%d:%ld, ", 1, (long)cache->log_eviction_age_cnt[0]);
  for (int i = 1; i < EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->log_eviction_age_cnt[i] == 0) {
      continue;
    }
    fprintf(ofile, "%lu:%ld, ", 1lu << i, (long)cache->log_eviction_age_cnt[i]);
  }
  fprintf(ofile, "\n\n");

  fclose(ofile);
  return true;
}

/**
 * @brief dump the eviction age distribution to a file
 *
 * @param cache
 * @param ofilepath
 * @return true
 * @return false
 */
bool dump_eviction_age(const cache_t *cache, const char *ofilepath) {
  FILE *ofile = fopen(ofilepath, "a");
  if (ofile == NULL) {
    perror("fopen failed");
    return false;
  }

  /* dump the objects' ages at eviction */
  fprintf(ofile, "%s, eviction age, cache size: %lu, ", cache->cache_name,
          (unsigned long)cache->cache_size);
  for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->log_eviction_age_cnt[i] == 0) {
      continue;
    }
    fprintf(ofile, "%lld:%ld, ", (long long)pow(EVICTION_AGE_LOG_BASE, i),
            (long)cache->log_eviction_age_cnt[i]);
  }
  fprintf(ofile, "\n");

  fclose(ofile);
  return true;
}

/**
 * @brief dump the age distribution of cached objects to a file
 *
 * WARNNING: this function obtain the age via evicting the cached objects
 * so the cache state will change after calling this function
 *
 * @param cache
 * @param req used to provide the current time
 * @param ofilepath
 * @return true
 * @return false
 */
bool dump_cached_obj_age(cache_t *cache, const request_t *req, const char *ofilepath) {
  FILE *ofile = fopen(ofilepath, "a");
  if (ofile == NULL) {
    perror("fopen failed");
    return false;
  }

  /* clear/reset eviction age counters */
  for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++) {
    cache->log_eviction_age_cnt[i] = 0;
  }

  int64_t n_cached_obj = cache->get_n_obj(cache);
  int64_t n_evicted_obj = 0;
  /* evict all the objects */
  while (cache->get_occupied_byte(cache) > 0) {
    cache->evict(cache, req, NULL);
    n_evicted_obj++;
  }
  assert(n_cached_obj == n_evicted_obj);

  int64_t n_ages = 0;
  /* dump the cached objects' ages */
  fprintf(ofile, "%s, cached_obj age, cache size: %lu, ", cache->cache_name,
          (unsigned long)cache->cache_size);
  for (int i = 0; i < EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->log_eviction_age_cnt[i] == 0) {
      continue;
    }
    n_ages += cache->log_eviction_age_cnt[i];
    fprintf(ofile, "%lld:%ld, ", (long long)pow(EVICTION_AGE_LOG_BASE, i),
            (long)cache->log_eviction_age_cnt[i]);
  }
  fprintf(ofile, "\n");
  assert(n_ages == n_cached_obj);

  fclose(ofile);
  return true;
}
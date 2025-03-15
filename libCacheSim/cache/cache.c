//
// Created by Juncheng Yang on 6/20/20.
//

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/cache.h"
#include "../include/libCacheSim/prefetchAlgo.h"

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
                           const common_cache_params_t params,
                           const void *const init_params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->cache_name, cache_name, CACHE_NAME_ARRAY_LEN);

  if (init_params != NULL) {
    strncpy(cache->init_params, init_params, CACHE_INIT_PARAMS_LEN);
  }
  cache->cache_size = params.cache_size;
  cache->eviction_params = NULL;
  cache->admissioner = NULL;
  cache->prefetcher = NULL;
  cache->future_stack_dist = NULL;
  cache->future_stack_dist_array_size = 0;
  cache->default_ttl = params.default_ttl;
  cache->n_req = 0;
  cache->to_evict_candidate = NULL;
  cache->to_evict_candidate_gen_vtime = -1;

  cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;

  /* this option works only when eviction age tracking
   * is on in config.h */
#if defined(TRACK_EVICTION_V_AGE)
  cache->track_eviction_age = true;
#endif
#if defined(TRACK_DEMOTION)
  cache->track_demotion = true;
#endif

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
  if (cache->prefetcher != NULL) cache->prefetcher->free(cache->prefetcher);
  my_free(sizeof(cache_t), cache);
}

/**
 * @brief create a new cache with the same size as the old cache
 *
 * @param old_cache
 * @param new_size
 * @return cache_t*
 */
cache_t *clone_cache(const cache_t *old_cache) {
  common_cache_params_t cc_params = {
      .cache_size = old_cache->cache_size,
      .hashpower = old_cache->hashtable->hashpower,
      .default_ttl = old_cache->default_ttl,
      .consider_obj_metadata = old_cache->obj_md_size == 0 ? false : true,
  };
  assert(sizeof(cc_params) == 24);
  cache_t *cache = old_cache->cache_init(cc_params, old_cache->init_params);
  if (old_cache->admissioner != NULL) {
    cache->admissioner = old_cache->admissioner->clone(old_cache->admissioner);
  }
  cache->future_stack_dist = old_cache->future_stack_dist;
  cache->future_stack_dist_array_size = old_cache->future_stack_dist_array_size;

  return cache;
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
      .consider_obj_metadata = old_cache->obj_md_size == 0 ? false : true,
  };
  assert(sizeof(cc_params) == 24);
  cache_t *cache = old_cache->cache_init(cc_params, old_cache->init_params);
  if (old_cache->admissioner != NULL) {
    cache->admissioner = old_cache->admissioner->clone(old_cache->admissioner);
  }
  if (old_cache->prefetcher != NULL) {
    cache->prefetcher =
        old_cache->prefetcher->clone(old_cache->prefetcher, new_size);
  }
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
          (long)cache->n_req, (unsigned long)req->obj_id,
          (unsigned long)req->obj_size);
      return false;
    }
  }

  if (req->obj_size + cache->obj_md_size > cache->cache_size) {
    WARN_ONCE("%ld req, obj %lu, size %lu larger than cache size %lu\n",
              (long)cache->n_req, (unsigned long)req->obj_id,
              (unsigned long)req->obj_size, (unsigned long)cache->cache_size);
    return false;
  }

  return true;
}

/**
 * @brief this function is called by eviction algorithms that use
 * the hash table to find whether an object is in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the number of requests increases by 1,
 *  the object size will be updated,
 *  and if the object is expired, it is removed from the cache
 * @return the found cache_obj_t* or NULL if not found
 */
cache_obj_t *cache_find_base(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  cache_obj_t *cache_obj = hashtable_find(cache->hashtable, req);

  // "update_cache = true" means that it is a real user request, use handle_find
  // to update prefetcher's state
  if (cache->prefetcher && cache->prefetcher->handle_find && update_cache) {
    bool hit = (cache_obj != NULL);
    cache->prefetcher->handle_find(cache, req, hit);
  }

  if (cache_obj != NULL) {
#ifdef SUPPORT_TTL
    if (cache_obj->exp_time != 0 && cache_obj->exp_time < req->clock_time) {
      if (update_cache) {
        cache->remove(cache, cache_obj->obj_id);
      }

      cache_obj = NULL;
    }
#endif

    if (update_cache) {
      cache_obj->misc.next_access_vtime = req->next_access_vtime;
      cache_obj->misc.freq += 1;
    }
  }

  return cache_obj;
}

/**
 * @brief this function is called by all eviction algorithms
 * it performs the following logic
 *
 * ```
 * if obj in cache:
 *    update_metadata
 *    return true
 * else:
 *    if cache does not have enough space:
 *        evict until it has space to insert
 *    insert the object
 *    return false
 * ```
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */
bool cache_get_base(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  VERBOSE("******* %s req %ld, obj %ld, obj_size %ld, cache size %ld/%ld\n",
          cache->cache_name, cache->n_req, req->obj_id, req->obj_size,
          cache->get_occupied_byte(cache), cache->cache_size);

  cache_obj_t *obj = cache->find(cache, req, true);
  bool hit = (obj != NULL);

  if (cache->admissioner && cache->admissioner->update) {
    cache->admissioner->update(cache->admissioner, req, cache->cache_size);
  }

  if (hit) {
    VVERBOSE("req %ld, obj %ld --- cache hit\n", cache->n_req, req->obj_id);
  } else if (!cache->can_insert(cache, req)) {
    VVERBOSE("req %ld, obj %ld --- cache miss cannot insert\n", cache->n_req,
             req->obj_id);
  } else {
    while (cache->get_occupied_byte(cache) + req->obj_size +
               cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }
    cache->insert(cache, req);
  }

  if (cache->prefetcher && cache->prefetcher->prefetch) {
    cache->prefetcher->prefetch(cache, req);
  }

  return hit;
}

/**
 * @brief this function is called by all caches to
 * insert an object into the cache, update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
cache_obj_t *cache_insert_base(cache_t *cache, const request_t *req) {
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache->occupied_byte +=
      (int64_t)cache_obj->obj_size + (int64_t)cache->obj_md_size;
  cache->n_obj += 1;

#ifdef SUPPORT_TTL
  if (cache->default_ttl != 0 && req->ttl == 0) {
    cache_obj->exp_time = (int32_t)cache->default_ttl + req->clock_time;
  }
#endif

#if defined(TRACK_EVICTION_V_AGE) || defined(TRACK_DEMOTION) || \
    defined(TRACK_CREATE_TIME)
  cache_obj->create_time = CURR_TIME(cache, req);
#endif

  cache_obj->misc.next_access_vtime = req->next_access_vtime;
  cache_obj->misc.freq = 0;

  return cache_obj;
}

/**
 * @brief this function is called by all eviction algorithms in the eviction
 * function, it updates the cache metadata. Because it frees the object struct,
 * it needs to be called at the end of the eviction function.
 *
 * @param cache the cache
 * @param obj the object to be removed
 */
void cache_evict_base(cache_t *cache, cache_obj_t *obj,
                      bool remove_from_hashtable) {
#if defined(TRACK_EVICTION_V_AGE)
  if (cache->track_eviction_age) {
    record_eviction_age(cache, obj, CURR_TIME(cache, req) - obj->create_time);
  }
#endif
  if (cache->prefetcher && cache->prefetcher->handle_evict) {
    request_t *check_req = new_request();
    check_req->obj_id = obj->obj_id;
    check_req->obj_size = obj->obj_size;
    // check_req->ttl = 0; // re-add objects should be?
    cache_remove_obj_base(cache, obj, remove_from_hashtable);
    cache->prefetcher->handle_evict(cache, check_req);
    my_free(sizeof(request_t), check_req);
    return;
  }

  cache_remove_obj_base(cache, obj, remove_from_hashtable);
}

/**
 * @brief this function is called by all eviction algorithms that
 * need to remove an object from the cache, it updates the cache metadata,
 * because it frees the object struct, it needs to be called at the end of
 * the eviction function.
 *
 * @param cache the cache
 * @param obj the object to be removed
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj,
                           bool remove_from_hashtable) {
  DEBUG_ASSERT(cache->occupied_byte >= obj->obj_size + cache->obj_md_size);
  cache->occupied_byte -= (obj->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;
  if (remove_from_hashtable) {
    hashtable_delete(cache->hashtable, obj);
  }
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
bool dump_cached_obj_age(cache_t *cache, const request_t *req,
                         const char *ofilepath) {
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
    cache->evict(cache, req);
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
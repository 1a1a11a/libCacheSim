
#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/evictionAlgo/Cacheus.h"
// SR_LRU is used by Cacheus.

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void SR_LRU_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void SR_LRU_free(cache_t *cache);
static bool SR_LRU_get(cache_t *cache, const request_t *req);
static cache_obj_t *SR_LRU_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *SR_LRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *SR_LRU_to_evict(cache_t *cache, const request_t *req);
static void SR_LRU_evict(cache_t *cache, const request_t *req);
static bool SR_LRU_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *SR_LRU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SR_LRU", ccache_params);
  cache->cache_init = SR_LRU_init;
  cache->cache_free = SR_LRU_free;
  cache->get = SR_LRU_get;
  cache->find = SR_LRU_find;
  cache->insert = SR_LRU_insert;
  cache->evict = SR_LRU_evict;
  cache->remove = SR_LRU_remove;
  cache->to_evict = SR_LRU_to_evict;
  cache->init_params = cache_specific_params;

  if (cache_specific_params != NULL) {
    printf("SR-LRU does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }
  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc_n(SR_LRU_params_t, 1);
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  params->req_local = new_request();
  params->other_cache = NULL;  // for Cacheus
  // 1/2 for each SR and R, 1 for H
  params->H_list = LRU_init(ccache_params, NULL);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size /= 2;
  params->SR_list = LRU_init(ccache_params_local, NULL);
  params->R_list = LRU_init(ccache_params_local, NULL);
  params->C_demoted = 0;
  params->C_new = 0;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void SR_LRU_free(cache_t *cache) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  params->H_list->cache_free(params->H_list);
  params->SR_list->cache_free(params->SR_list);
  params->R_list->cache_free(params->R_list);
  my_free(sizeof(SR_LRU_params_t), params);
  cache_struct_free(cache);
}

/**
 * @brief this function is the user facing API
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
static bool SR_LRU_get(cache_t *cache, const request_t *req) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  bool cache_hit = SR_LRU_find(cache, req, true) != NULL;

  if (!cache_hit) {
    if (cache->can_insert(cache, req)) {
      // TODO (jason): eviction logic should move moved out of insert
      SR_LRU_insert(cache, req);
    }
    return false;
  }
  return true;
  // TODO (jason): is there a bug here? we did not increase cache->n_req,
  // and should we use the following instead?
  // return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *SR_LRU_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  cache_t *R = params->R_list;
  cache_t *SR = params->SR_list;
  // SR_LRU_find will cover cases where:
  // Hit in Cache (R) and hit in Cache (SR) and does not hit anything
  cache_obj_t *obj_R = R->find(R, req, update_cache);
  bool cache_hit_R = (obj_R != NULL);
  bool cache_hit_SR = (SR->find(SR, req, false) != NULL);
  DEBUG_ASSERT((cache_hit_R ? 1 : 0) + (cache_hit_SR ? 1 : 0) <= 1);

  if (cache_hit_R && likely(update_cache)) {
    // On a cache hit where requested obj is in R, it is moved to the MRU
    // position of R, and it has just been done.
    ;
  } else if (cache_hit_SR && likely(update_cache)) {
    // On a cache hit where requested obj is in SR, it is moved to the MRU
    // position of R. Move hit obj from SR to R.
    SR->remove(SR, req->obj_id);

    // If R list is full, move obj from R to SR.
    while (R->get_occupied_byte(R) + req->obj_size + cache->obj_md_size >
           R->cache_size) {
      DEBUG_ASSERT(R->get_occupied_byte(R) > 0);
      cache_obj_t *obj_from_R = R->to_evict(R, req);
      copy_cache_obj_to_request(params->req_local, obj_from_R);
      cache_obj_t *obj_in_SR = SR->insert(SR, params->req_local);
      if (!obj_from_R->SR_LRU.demoted) {
        params->C_demoted += 1;
        obj_in_SR->SR_LRU.demoted = true;
      }
      R->evict(R, req);
    }
    obj_R = R->insert(R, req);
  }

  // Dynamic size adjustment.
  // If req is demoted from R before
  // alg infers that the size of R is too small and needs to be increased.
  if (likely(update_cache) && cache_hit_SR && obj_R->SR_LRU.demoted) {
    double delta;
    DEBUG_ASSERT(params->C_demoted >= 1);

    if (1.0 > (int)(params->C_new / params->C_demoted) + 0.5)
      delta = 1.0;
    else
      delta = (int)(params->C_new / params->C_demoted) + 0.5;

    if (params->SR_list->cache_size - delta >
        1.0)  // reduce size of SR by delta;
      params->SR_list->cache_size -= delta;
    else
      params->SR_list->cache_size = 1.0;

    params->R_list->cache_size =
        params->H_list->cache_size - params->SR_list->cache_size;
    obj_R->SR_LRU.demoted = false;
    params->C_demoted -= 1;
  }

  if (cache_hit_R || cache_hit_SR) {
    // Update cache_size
    cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
    cache->occupied_byte =
        params->SR_list->occupied_byte + params->R_list->occupied_byte;
    DEBUG_ASSERT(obj_R != NULL);
    return obj_R;
  }
  return NULL;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *SR_LRU_insert(cache_t *cache, const request_t *req) {
  // SR_LRU_insert covers the cases where hit in history or does not hit
  // anything.
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  bool ck_hist = params->H_list->find(params->H_list, req, false) != NULL;
  DEBUG_ASSERT(req->obj_size + cache->obj_md_size <
               params->SR_list->cache_size);
  // If history hit
  if (ck_hist) {
    // On a cache miss where x is in H, x is moved to the MRU position of R.
    params->H_list->remove(params->H_list, req->obj_id);

    // If R list is full, move obj from R to SR.
    while (params->R_list->occupied_byte + req->obj_size + cache->obj_md_size >
           params->R_list->cache_size) {
      DEBUG_ASSERT(params->R_list->occupied_byte != 0);

      cache_obj_t *evicted_obj = params->R_list->to_evict(params->R_list, req);
      copy_cache_obj_to_request(req_local, evicted_obj);
      params->SR_list->insert(params->SR_list, req_local);

      // Mark the obj as demoted
      if (!evicted_obj->SR_LRU.demoted) {
        params->C_demoted += 1;
        // evicted_obj.SR_LRU.demoted = true;
        params->SR_list->find(params->SR_list, req_local, false)
            ->SR_LRU.demoted = true;
      }
      params->R_list->evict(params->R_list, req);
    }

    params->R_list->insert(params->R_list, req);
    cache_obj_t *obj = params->R_list->find(params->R_list, req, false);

    // Dynamic size adjustment
    // If an obj is moved from H to R
    // This means that SR cache size is too small and needs to be increases;
    if (obj->SR_LRU.new_obj) {
      DEBUG_ASSERT(params->C_new >= 1);
      double delta;
      if (1.0 > (int)(params->C_demoted / params->C_new) + 0.5)
        delta = 1.0;
      else
        delta = (int)(params->C_demoted / params->C_new) + 0.5;

      if (params->SR_list->cache_size + delta > cache->occupied_byte - 1)
        params->SR_list->cache_size = cache->occupied_byte - 1;
      else
        params->SR_list->cache_size += delta;

      params->R_list->cache_size =
          params->H_list->cache_size - params->SR_list->cache_size;
      obj->SR_LRU.new_obj = false;
    }
  } else {
    // cache miss, history miss
    params->SR_list->insert(params->SR_list, req);
    cache_obj_t *obj = params->SR_list->find(params->SR_list, req, false);

    // label that obj as new obj;
    obj->SR_LRU.new_obj = true;
    DEBUG_ASSERT(params->SR_list->to_evict);
  }

  // If SR is full
  while (params->SR_list->occupied_byte > params->SR_list->cache_size) {
    // The LRU item of SR is evicted to H.
    cache_obj_t *obj_to_evict = params->SR_list->to_evict(params->SR_list, req);
    copy_cache_obj_to_request(req_local, obj_to_evict);
    params->H_list->insert(params->H_list, req_local);

    if (params->other_cache) {
      params->other_cache->remove(params->other_cache, obj_to_evict->obj_id);
    }
    if (obj_to_evict->SR_LRU.new_obj) {
      params->C_new += 1;  // increment the number of new objs in history
      params->H_list->find(params->H_list, req_local, false)->SR_LRU.new_obj =
          true;
    }
    if (obj_to_evict->SR_LRU.demoted) {
      // obj_to_evict.SR_LRU.demoted = false;
      params->H_list->find(params->H_list, req_local, false)->SR_LRU.demoted =
          false;
      params->C_demoted -= 1;
    }
    params->SR_list->evict(params->SR_list, req);
    obj_to_evict = NULL;
  }
  // If H is full
  while (params->H_list->occupied_byte >= params->H_list->cache_size) {
    params->H_list->evict(params->H_list, req);
  }
  cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
  cache->occupied_byte =
      params->SR_list->occupied_byte + params->R_list->occupied_byte;

  return NULL;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *SR_LRU_to_evict(cache_t *cache, const request_t *req) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  cache->to_evict_candidate = params->SR_list->to_evict(params->SR_list, req);
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  return cache->to_evict_candidate;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void SR_LRU_evict(cache_t *cache, const request_t *req) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  cache_t *R = params->R_list;
  cache_t *SR = params->SR_list;
  cache_t *H = params->H_list;

  cache_obj_t *obj_to_evict = SR_LRU_to_evict(cache, req);
  assert(obj_to_evict != NULL);
  copy_cache_obj_to_request(params->req_local, obj_to_evict);
  // SR eviction happens later
  cache_obj_t *obj_inserted = H->insert(H, params->req_local);

  if (obj_to_evict->SR_LRU.new_obj) {  // if evicted obj is new
    params->C_new += 1;  // increment the number of new objs in hist
    obj_inserted->SR_LRU.new_obj = true;
  }
  if (obj_to_evict->SR_LRU.demoted) {
    params->C_demoted -= 1;  // decrement the number of demoted objs in cache
    obj_inserted->SR_LRU.demoted = true;
    // obj_to_evict->SR_LRU.demoted = false;
  }
  SR->evict(SR, req);

  while (H->get_occupied_byte(H) >= H->cache_size) {
    H->evict(H, req);
  }

  cache->n_obj = SR->get_n_obj(SR) + R->get_n_obj(R);
  cache->occupied_byte = SR->get_occupied_byte(SR) + R->get_occupied_byte(R);
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool SR_LRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);

  cache_t *R = params->R_list;
  cache_t *SR = params->SR_list;
  cache_t *H = params->H_list;

  bool in_R = false, in_SR = false;
  params->req_local->obj_id = obj_id;
  cache_obj_t *obj = R->find(R, params->req_local, false);
  if (obj != NULL) {
    in_R = true;
    copy_cache_obj_to_request(params->req_local, obj);
  } else {
    obj = SR->find(SR, params->req_local, false);
    if (obj == NULL) {
      return false;
    }
    in_SR = true;
    copy_cache_obj_to_request(params->req_local, obj);
  }

  // Remove should remove the obj and push it to history
  cache_obj_t *obj_in_hist = H->insert(H, params->req_local);

  if (obj->SR_LRU.new_obj) {  // if evicted obj is new
    params->C_new += 1;       // increment the number of new objs in hist
    obj_in_hist->SR_LRU.new_obj = true;
  }
  if (obj->SR_LRU.demoted) {
    params->C_demoted -= 1;  // decrement the number of demoted objs in cache
    obj_in_hist->SR_LRU.demoted = false;
  }

  if (in_R) {
    R->remove(R, obj_id);
  } else {
    SR->remove(SR, obj_id);
  }
  cache->n_obj = SR->n_obj + R->n_obj;
  cache->occupied_byte = SR->occupied_byte + R->occupied_byte;

  while (H->get_occupied_byte(H) >= H->cache_size) {
    H->evict(H, NULL);
  }

  return true;

  // cache_obj_t *obj = cache_get_obj_by_id(params->SR_list, obj_id);
  // bool remove_from_SR = false;
  // if (obj) {
  //   LRU_params_t *lru_params =
  //       (LRU_params_t *)(params->SR_list->eviction_params);
  //   remove_obj_from_list(&lru_params->q_head, &lru_params->q_tail, obj);
  //   remove_from_SR = true;
  // } else {
  //   obj = cache_get_obj_by_id(params->R_list, obj_id);
  //   LRU_params_t *lru_params =
  //       (LRU_params_t *)(params->R_list->eviction_params);
  //   remove_obj_from_list(&lru_params->q_head, &lru_params->q_tail, obj);
  //   remove_from_SR = false;
  // }

  // Remove should remove the obj and push it to history
  // copy_cache_obj_to_request(params->req_local, obj);
  // params->H_list->insert(params->H_list, params->req_local);
  // cache_obj_t *obj_in_hist = cache_get_obj_by_id(params->H_list, obj_id);

  // if (obj->SR_LRU.new_obj) {  // if evicted obj is new
  //   params->C_new += 1;       // increment the number of new objs in hist
  //   obj_in_hist->SR_LRU.new_obj = true;
  // }
  // if (obj->SR_LRU.demoted) {
  //   params->C_demoted -= 1;  // decrement the number of demoted objs in cache
  //   obj_in_hist->SR_LRU.demoted = false;
  // }

  // if (remove_from_SR) {
  //   cache_remove_obj_base(params->SR_list, obj, true);
  // } else {
  //   cache_remove_obj_base(params->R_list, obj, true);
  // }
  // cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
  // cache->occupied_byte =
  //     params->SR_list->occupied_byte + params->R_list->occupied_byte;

  // while (params->H_list->occupied_byte >= params->H_list->cache_size) {
  //   cache_obj_t evicted_obj;
  //   params->H_list->evict(params->H_list, params->req_local, &evicted_obj);
  // }

  // if (obj == NULL) {
  //   return false;
  // }

  // return true;
}

#ifdef __cplusplus
}
#endif

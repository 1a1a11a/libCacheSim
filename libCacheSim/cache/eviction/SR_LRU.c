
#include "../../include/libCacheSim/evictionAlgo/SR_LRU.h"

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/LRU.h"
// SR_LRU is used by Cacheus.

#ifdef __cplusplus
extern "C" {
#endif

cache_t *SR_LRU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SR_LRU", ccache_params);
  cache->cache_init = SR_LRU_init;
  cache->cache_free = SR_LRU_free;
  cache->get = SR_LRU_get;
  cache->check = SR_LRU_check;
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
    cache->per_obj_metadata_size = 2;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  cache->eviction_params = my_malloc_n(SR_LRU_params_t, 1);
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
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

void SR_LRU_free(cache_t *cache) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  params->H_list->cache_free(params->H_list);
  params->SR_list->cache_free(params->SR_list);
  params->R_list->cache_free(params->R_list);
  my_free(sizeof(SR_LRU_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e SR_LRU_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
  // SR_LRU_check will cover cases where:
  // Hit in Cache (R) and hit in Cache (SR) and does not hit anything
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  cache_ck_res_e ck_R = params->R_list->check(params->R_list, req, false);
  cache_ck_res_e ck_sr = params->SR_list->check(params->SR_list, req, false);

  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  DEBUG_ASSERT(
      (ck_R == cache_ck_hit ? 1 : 0) + (ck_sr == cache_ck_hit ? 1 : 0) <= 1);
  // On a cache hit where requested obj is in R, x is moved to the MRU position
  // of R.
  if (ck_R == cache_ck_hit) {
    params->R_list->check(params->R_list, req, update_cache);
  }

  // On a cache hit where x is in SR, requested obj is moved to the MRU position
  // of R
  else if (ck_sr == cache_ck_hit && likely(update_cache)) {
    // Move hit obj from SR to R.
    params->SR_list->remove(params->SR_list, req->obj_id);

    // If R list is full, move obj from R to SR.
    while (params->R_list->occupied_size + req->obj_size +
               cache->per_obj_metadata_size >
           params->R_list->cache_size) {
      DEBUG_ASSERT(params->R_list->occupied_size != 0);
      cache_obj_t evicted_obj;
      LRU_evict(params->R_list, req, &evicted_obj);
      copy_cache_obj_to_request(req_local, &evicted_obj);

      LRU_insert(params->SR_list, req_local);
      if (!evicted_obj.SR_LRU.demoted) {
        params->C_demoted += 1;
        // evicted_obj.SR_LRU.demoted = true;
        cache_get_obj_by_id(params->SR_list, req_local->obj_id)
            ->SR_LRU.demoted = true;
      }
    }
    params->R_list->insert(params->R_list, req);
  }

  // Dynamic size adjustment.
  // If req is demoted from R before
  // alg infers that the size of R is too small and needs to be increased.
  cache_obj_t *obj = cache_get_obj_by_id(params->R_list, req->obj_id);
  if (ck_sr == cache_ck_hit && obj->SR_LRU.demoted && likely(update_cache)) {
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
    obj->SR_LRU.demoted = false;
    params->C_demoted -= 1;
  }

  if (ck_R == cache_ck_hit || ck_sr == cache_ck_hit) {
    // Update cache_size
    cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
    cache->occupied_size =
        params->SR_list->occupied_size + params->R_list->occupied_size;
    return cache_ck_hit;
  }
  return ck_R;
}

cache_ck_res_e SR_LRU_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e ret;
  ret = SR_LRU_check(cache, req, true);
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);

  if (ret == cache_ck_miss) {
    if (req->obj_size + cache->per_obj_metadata_size > params->SR_list->cache_size) {
      return ret;
    }
    SR_LRU_insert(cache, req);
  }
  return ret;
}

cache_obj_t *SR_LRU_insert(cache_t *cache, const request_t *req) {
  // SR_LRU_insert covers the cases where hit in history or does not hit
  // anything.
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  cache_ck_res_e ck_hist = params->H_list->check(params->H_list, req, false);
  static __thread request_t *req_local = NULL;
  DEBUG_ASSERT(req->obj_size + cache->per_obj_metadata_size <
               params->SR_list->cache_size);
  if (req_local == NULL) {
    req_local = new_request();
  }
  // If history hit
  if (ck_hist == cache_ck_hit) {
    // On a cache miss where x is in H, x is moved to the MRU position of R.
    params->H_list->remove(params->H_list, req->obj_id);

    // If R list is full, move obj from R to SR.
    while (params->R_list->occupied_size + req->obj_size +
               cache->per_obj_metadata_size >
           params->R_list->cache_size) {
      DEBUG_ASSERT(params->R_list->occupied_size != 0);

      cache_obj_t evicted_obj;
      LRU_evict(params->R_list, req, &evicted_obj);
      copy_cache_obj_to_request(req_local, &evicted_obj);
      LRU_insert(params->SR_list, req_local);

      // Mark the obj as demoted
      if (!evicted_obj.SR_LRU.demoted) {
        params->C_demoted += 1;
        // evicted_obj.SR_LRU.demoted = true;
        cache_get_obj_by_id(params->SR_list, req_local->obj_id)
            ->SR_LRU.demoted = true;
      }
    }

    params->R_list->insert(params->R_list, req);
    cache_obj_t *obj = cache_get_obj_by_id(params->R_list, req->obj_id);

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

      if (params->SR_list->cache_size + delta > cache->occupied_size - 1)
        params->SR_list->cache_size = cache->occupied_size - 1;
      else
        params->SR_list->cache_size += delta;

      params->R_list->cache_size =
          params->H_list->cache_size - params->SR_list->cache_size;
      obj->SR_LRU.new_obj = false;
    }
  } else {
    // cache miss, history miss
    params->SR_list->insert(params->SR_list, req);
    cache_obj_t *obj = cache_get_obj_by_id(params->SR_list, req->obj_id);

    // label that obj as new obj;
    obj->SR_LRU.new_obj = true;
    DEBUG_ASSERT(params->SR_list->to_evict);
  }

  // If SR is full
  while (params->SR_list->occupied_size > params->SR_list->cache_size) {
    // The LRU item of SR is evicted to H.
    cache_obj_t evicted_obj;
    params->SR_list->evict(params->SR_list, req, &evicted_obj);
    copy_cache_obj_to_request(req_local, &evicted_obj);
    params->H_list->insert(params->H_list, req_local);

    if (params->other_cache)
      params->other_cache->remove(params->other_cache, evicted_obj.obj_id);

    if (evicted_obj.SR_LRU.new_obj) {
      params->C_new += 1;  // increment the number of new objs in history
      cache_get_obj_by_id(params->H_list, req_local->obj_id)->SR_LRU.new_obj =
          true;
    }
    if (evicted_obj.SR_LRU.demoted) {
      // evicted_obj.SR_LRU.demoted = false;
      cache_get_obj_by_id(params->H_list, req_local->obj_id)->SR_LRU.demoted =
          false;
      params->C_demoted -= 1;
    }
    DEBUG_ASSERT(params->SR_list->to_evict);
  }
  // If H is full
  while (params->H_list->occupied_size >= params->H_list->cache_size) {
    cache_obj_t evicted_obj;
    params->H_list->evict(params->H_list, req, &evicted_obj);
  }
  cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
  cache->occupied_size =
      params->SR_list->occupied_size + params->R_list->occupied_size;

  return NULL;
}

cache_obj_t *SR_LRU_to_evict(cache_t *cache) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  if (params->SR_list->to_evict(params->SR_list))
    return params->SR_list->to_evict(params->SR_list);
  else {
    DEBUG_ASSERT(0);
  }
}

void SR_LRU_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  if (params->SR_list->to_evict(params->SR_list)) {
    params->SR_list->evict(params->SR_list, req, evicted_obj);
  } else {
    DEBUG_ASSERT(0);
  }

  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  copy_cache_obj_to_request(req_local, evicted_obj);
  params->H_list->insert(params->H_list, req_local);

  if (evicted_obj->SR_LRU.new_obj) {  // if evicted obj is new
    params->C_new += 1;  // increment the number of new objs in hist
    cache_get_obj_by_id(params->H_list, req_local->obj_id)->SR_LRU.new_obj =
        true;
  }
  if (evicted_obj->SR_LRU.demoted) {
    params->C_demoted -= 1;  // decrement the number of demoted objs in cache
    cache_get_obj_by_id(params->H_list, req_local->obj_id)->SR_LRU.demoted =
        true;
    // evicted_obj->SR_LRU.demoted = false;
  }

  while (params->H_list->occupied_size >= params->H_list->cache_size) {
    cache_obj_t evicted_obj_tmp;
    params->H_list->evict(params->H_list, req, &evicted_obj_tmp);
  }

  cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
  cache->occupied_size =
      params->SR_list->occupied_size + params->R_list->occupied_size;
}

void SR_LRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SR_LRU_params_t *params = (SR_LRU_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  cache_obj_t *obj = cache_get_obj_by_id(params->SR_list, obj_id);
  bool remove_from_SR = false;
  if (obj) {
    remove_obj_from_list(&(params->SR_list)->q_head, &(params->SR_list)->q_tail,
                         obj);
    remove_from_SR = true;
  } else {
    obj = cache_get_obj_by_id(params->R_list, obj_id);
    DEBUG_ASSERT(obj != NULL);
    remove_obj_from_list(&(params->R_list)->q_head, &(params->R_list)->q_tail,
                         obj);
    remove_from_SR = false;
  }

  // Remove should remove the obj and push it to history
  copy_cache_obj_to_request(req_local, obj);
  params->H_list->insert(params->H_list, req_local);
  cache_obj_t *obj_in_hist = cache_get_obj_by_id(params->H_list, obj_id);

  if (obj->SR_LRU.new_obj) {  // if evicted obj is new
    params->C_new += 1;       // increment the number of new objs in hist
    obj_in_hist->SR_LRU.new_obj = true;
  }
  if (obj->SR_LRU.demoted) {
    params->C_demoted -= 1;  // decrement the number of demoted objs in cache
    obj_in_hist->SR_LRU.demoted = false;
  }

  if (remove_from_SR) {
    cache_remove_obj_base(params->SR_list, obj);
  } else {
    cache_remove_obj_base(params->R_list, obj);
  }
  cache->n_obj = params->SR_list->n_obj + params->R_list->n_obj;
  cache->occupied_size =
      params->SR_list->occupied_size + params->R_list->occupied_size;

  while (params->H_list->occupied_size >= params->H_list->cache_size) {
    cache_obj_t evicted_obj;
    params->H_list->evict(params->H_list, req_local, &evicted_obj);
  }

  if (obj == NULL) {
    WARN("obj (%" PRIu64 ") to remove is not in the cache\n", obj_id);
    return;
  }
}

#ifdef __cplusplus
}
#endif

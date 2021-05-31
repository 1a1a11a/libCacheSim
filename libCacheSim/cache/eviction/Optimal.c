//
//  the Optimal eviction algorithm (Belady) this is optimal for unit size object
//
//
//  Optimal.c
//  libCacheSim
//
//
// Created by Juncheng Yang on 3/30/21.
//

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/Optimal.h"

#ifdef __cplusplus
extern "C" {
#endif

void Optimal_remove_obj(cache_t *cache, cache_obj_t *obj);


cache_t *Optimal_init(common_cache_params_t ccache_params,
                      __attribute__((unused)) void *init_params) {
  cache_t *cache = cache_struct_init("Optimal", ccache_params);
  cache->cache_init = Optimal_init;
  cache->cache_free = Optimal_free;
  cache->get = Optimal_get;
  cache->check = Optimal_check;
  cache->insert = Optimal_insert;
  cache->evict = Optimal_evict;
  cache->remove = Optimal_remove;

  Optimal_params_t *params = my_malloc(Optimal_params_t);
  cache->eviction_params = params;

  params->pq = pqueue_init((unsigned long) 8e6);
  return cache;
}

void Optimal_free(cache_t *cache) {
  Optimal_params_t *params = cache->eviction_params;
  pq_node_t *node = pqueue_pop(params->pq);
  while (node) {
    my_free(sizeof(pq_node_t), node);
    node = pqueue_pop(params->pq);
  }
  pqueue_free(params->pq);

  cache_struct_free(cache);
}

cache_ck_res_e Optimal_check(cache_t *cache,
                             request_t *req,
                             bool update_cache) {
  Optimal_params_t *params = cache->eviction_params;
  cache_obj_t *cached_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cached_obj);

  if (ret == cache_ck_miss) {
    DEBUG_ASSERT(cache_get_obj(cache, req) == NULL);
  }

  if (!update_cache)
    return ret;

  if (ret == cache_ck_hit) {
    /* update next access ts, we use INT64_MAX - 10 because we reserve the largest elements for immediate delete */
    if (req->next_access_ts == -1 || req->next_access_ts == INT64_MAX) {
      Optimal_remove_obj(cache, cached_obj);
    } else {
      pqueue_pri_t pri = {.pri = req->next_access_ts};
      pqueue_change_priority(params->pq,
                             pri,
                             (pq_node_t *) (cached_obj->optimal.pq_node));
      DEBUG_ASSERT(((pq_node_t *) cache_get_obj(cache,
                                                req)->optimal.pq_node)->pri.pri
                       == req->next_access_ts);
    }
    return cache_ck_hit;
  } else if (ret == cache_ck_expired) {
    Optimal_remove_obj(cache, cached_obj);

    return cache_ck_miss;
  }

  return cache_ck_miss;
}

cache_ck_res_e Optimal_get(cache_t *cache, request_t *req) {
  /* -2 means the trace does not have next_access ts information */
  DEBUG_ASSERT(req->next_access_ts != -2);
  Optimal_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(cache->n_obj == params->pq->size - 1);
  cache_ck_res_e ret = cache_get_base(cache, req);

  return ret;
}

void Optimal_insert(cache_t *cache, request_t *req) {
  Optimal_params_t *params = cache->eviction_params;

  if (req->next_access_ts == -1) {
    return;
  }

  DEBUG_ASSERT(cache_get_obj(cache, req) == NULL);
  cache_obj_t *cached_obj = cache_insert_base(cache, req);

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id;
  node->pri.pri = req->next_access_ts;
  pqueue_insert(params->pq, (void *) node);
  cached_obj->optimal.pq_node = node;

  DEBUG_ASSERT(
      ((pq_node_t *) cache_get_obj(cache, req)->optimal.pq_node)->pri.pri
          == req->next_access_ts);
}

void Optimal_evict(cache_t *cache,
                   __attribute__((unused)) request_t *req,
                   __attribute__((unused)) cache_obj_t *evicted_obj) {
  Optimal_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *) pqueue_pop(params->pq);

  cache_obj_t *cached_obj = cache_get_obj_by_id(cache, node->obj_id);
  DEBUG_ASSERT(node == cached_obj->optimal.pq_node);

#ifdef TRACK_EVICTION_AGE
  record_eviction_age(cache, (int) (req->real_time - cached_obj->last_access_rtime));
#endif

  cached_obj->optimal.pq_node = NULL;
  my_free(sizeof(pq_node_t), node);

  Optimal_remove_obj(cache, cached_obj);
}

void Optimal_remove_obj(cache_t *cache, cache_obj_t *obj) {
  Optimal_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(hashtable_find_obj(cache->hashtable, obj) == obj);
  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);

  if (obj->optimal.pq_node != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj->optimal.pq_node);
    my_free(sizeof(pq_node_t), obj->optimal.pq_node);
    obj->optimal.pq_node = NULL;
  }

  cache_remove_obj_base(cache, obj);
}

void Optimal_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }

  Optimal_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

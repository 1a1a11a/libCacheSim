//
//  the Belady eviction algorithm (Belady) this is Belady for unit size object
//
//
//  Belady.c
//  libCacheSim
//
//
// Created by Juncheng Yang on 3/30/21.
//

#include "../../include/libCacheSim/evictionAlgo/Belady.h"

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../dataStructure/pqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Belady_params {
  pqueue_t *pq;
} Belady_params_t;

void Belady_remove_obj(cache_t *cache, cache_obj_t *obj);

cache_t *Belady_init(const common_cache_params_t ccache_params,
                     __attribute__((unused))
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Belady", ccache_params);
  cache->cache_init = Belady_init;
  cache->cache_free = Belady_free;
  cache->get = Belady_get;
  cache->check = Belady_check;
  cache->insert = Belady_insert;
  cache->evict = Belady_evict;
  cache->to_evict = Belady_to_evict;
  cache->remove = Belady_remove;
  cache->init_params = cache_specific_params;

  if (cache_specific_params != NULL) {
    printf("Belady does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  Belady_params_t *params = my_malloc(Belady_params_t);
  cache->eviction_params = params;

  params->pq = pqueue_init((unsigned long)8e6);
  return cache;
}

void Belady_free(cache_t *cache) {
  Belady_params_t *params = cache->eviction_params;
  pq_node_t *node = pqueue_pop(params->pq);
  while (node) {
    my_free(sizeof(pq_node_t), node);
    node = pqueue_pop(params->pq);
  }
  pqueue_free(params->pq);

  cache_struct_free(cache);
}

cache_ck_res_e Belady_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
  Belady_params_t *params = cache->eviction_params;
  cache_obj_t *cached_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cached_obj);

  if (!update_cache) return ret;

  if (ret == cache_ck_hit) {
    /* update next access ts, we use INT64_MAX - 10 because we reserve the
     * largest elements for immediate delete */
    if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
      Belady_remove_obj(cache, cached_obj);
    } else {
      pqueue_pri_t pri = {.pri = req->next_access_vtime};
      pqueue_change_priority(params->pq, pri,
                             (pq_node_t *)(cached_obj->Belady.pq_node));
      DEBUG_ASSERT(
          ((pq_node_t *)cache_get_obj(cache, req)->Belady.pq_node)->pri.pri ==
          req->next_access_vtime);
    }
    return cache_ck_hit;
  }

  return cache_ck_miss;
}

cache_ck_res_e Belady_get(cache_t *cache, const request_t *req) {
  /* -2 means the trace does not have next_access ts information */
  DEBUG_ASSERT(req->next_access_vtime != -2);
  Belady_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(cache->n_obj == params->pq->size - 1);
  cache_ck_res_e ret = cache_get_base(cache, req);

  return ret;
}

cache_obj_t *Belady_insert(cache_t *cache, const request_t *req) {
  Belady_params_t *params = cache->eviction_params;

  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
    record_eviction_age(cache, 0);
#endif

    return NULL;
  }

  cache_obj_t *cached_obj = cache_insert_base(cache, req);

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id;
  node->pri.pri = req->next_access_vtime;
  pqueue_insert(params->pq, (void *)node);
  cached_obj->Belady.pq_node = node;

  DEBUG_ASSERT(
      ((pq_node_t *)cache_get_obj(cache, req)->Belady.pq_node)->pri.pri ==
      req->next_access_vtime);
    
  return cached_obj;
}

cache_obj_t *Belady_to_evict(cache_t *cache) {
  Belady_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_peek(params->pq);
  return cache_get_obj_by_id(cache, node->obj_id);
}

void Belady_evict(cache_t *cache, __attribute__((unused)) const request_t *req,
                  __attribute__((unused)) cache_obj_t *evicted_obj) {
  Belady_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_pop(params->pq);

  cache_obj_t *obj_to_evict = cache_get_obj_by_id(cache, node->obj_id);
  DEBUG_ASSERT(node == obj_to_evict->Belady.pq_node);

#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(cache, req->real_time - obj_to_evict->create_time);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(cache, cache->n_req - obj_to_evict->create_time);
#endif

  obj_to_evict->Belady.pq_node = NULL;
  my_free(sizeof(pq_node_t), node);

  Belady_remove_obj(cache, obj_to_evict);
}

void Belady_remove_obj(cache_t *cache, cache_obj_t *obj) {
  Belady_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(hashtable_find_obj(cache->hashtable, obj) == obj);
  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);

  if (obj->Belady.pq_node != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj->Belady.pq_node);
    my_free(sizeof(pq_node_t), obj->Belady.pq_node);
    obj->Belady.pq_node = NULL;
  }

  cache_remove_obj_base(cache, obj);
}

void Belady_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    WARN("obj to remove is not in the cache\n");
    return;
  }

  Belady_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

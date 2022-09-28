/* Hyperbolic caching */

#include "../include/libCacheSim/evictionAlgo/Hyperbolic.h"

#include "../../dataStructure/pqueue.h"
#include "../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Hyperbolic_params {
  pqueue_t *pq;
} Hyperbolic_params_t;

void Hyperbolic_remove_obj(cache_t *cache, cache_obj_t *obj);

cache_t *Hyperbolic_init(const common_cache_params_t ccache_params,
                         __attribute__((unused))
                         const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Hyperbolic", ccache_params);
  cache->cache_init = Hyperbolic_init;
  cache->cache_free = Hyperbolic_free;
  cache->get = Hyperbolic_get;
  cache->check = Hyperbolic_check;
  cache->insert = Hyperbolic_insert;
  cache->evict = Hyperbolic_evict;
  cache->remove = Hyperbolic_remove;
  cache->to_evict = Hyperbolic_to_evict;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }
  if (ccache_params.consider_obj_metadata) {
    // freq + age
    cache->per_obj_metadata_size = 8 + 8;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  Hyperbolic_params_t *params = my_malloc(Hyperbolic_params_t);
  cache->eviction_params = params;

  params->pq = pqueue_init((unsigned long)8e6);
  return cache;
}

void Hyperbolic_free(cache_t *cache) {
  Hyperbolic_params_t *params = cache->eviction_params;
  pq_node_t *node = pqueue_pop(params->pq);
  while (node) {
    my_free(sizeof(pq_node_t), node);
    node = pqueue_pop(params->pq);
  }
  pqueue_free(params->pq);

  cache_struct_free(cache);
}

cache_ck_res_e Hyperbolic_check(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  Hyperbolic_params_t *params = cache->eviction_params;
  cache_obj_t *cached_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cached_obj);

  if (!update_cache) return ret;

  if (ret == cache_ck_hit) {
    pqueue_pri_t pri;
    int64_t age = cache->n_req - cached_obj->hyperbolic.vtime_enter_cache;
    pri.pri = (double)age / (double)(++cached_obj->hyperbolic.freq);
    pqueue_change_priority(params->pq, pri,
                           (pq_node_t *)(cached_obj->hyperbolic.pq_node));

    return cache_ck_hit;
  } else if (ret == cache_ck_expired) {
    Hyperbolic_remove_obj(cache, cached_obj);

    return cache_ck_expired;
  }

  return cache_ck_miss;
}

cache_ck_res_e Hyperbolic_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e ret = cache_get_base(cache, req);

  return ret;
}

void Hyperbolic_insert(cache_t *cache, const request_t *req) {
  Hyperbolic_params_t *params = cache->eviction_params;

  cache_obj_t *cached_obj = cache_insert_base(cache, req);
  cached_obj->hyperbolic.freq = 1;
  cached_obj->hyperbolic.vtime_enter_cache = cache->n_req;

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id;
  node->pri.pri = (double)req->obj_size;
  pqueue_insert(params->pq, (void *)node);
  cached_obj->hyperbolic.pq_node = node;
}

cache_obj_t *Hyperbolic_to_evict(cache_t *cache) {
  Hyperbolic_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_peek(params->pq);

  return cache_get_obj_by_id(cache, node->obj_id);
}

void Hyperbolic_evict(cache_t *cache,
                      __attribute__((unused)) const request_t *req,
                      __attribute__((unused)) cache_obj_t *evicted_obj) {
  Hyperbolic_params_t *params = cache->eviction_params;
  pq_node_t *node = (pq_node_t *)pqueue_pop(params->pq);

  cache_obj_t *cached_obj = cache_get_obj_by_id(cache, node->obj_id);
  DEBUG_ASSERT(node == cached_obj->hyperbolic.pq_node);

  cached_obj->hyperbolic.pq_node = NULL;
  my_free(sizeof(pq_node_t), node);

  Hyperbolic_remove_obj(cache, cached_obj);
}

void Hyperbolic_remove_obj(cache_t *cache, cache_obj_t *obj) {
  Hyperbolic_params_t *params = cache->eviction_params;

  DEBUG_ASSERT(hashtable_find_obj(cache->hashtable, obj) == obj);
  DEBUG_ASSERT(cache->occupied_size >= obj->obj_size);

  if (obj->hyperbolic.pq_node != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj->hyperbolic.pq_node);
    my_free(sizeof(pq_node_t), obj->hyperbolic.pq_node);
    obj->hyperbolic.pq_node = NULL;
  }

  cache_remove_obj_base(cache, obj);
}

void Hyperbolic_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("obj to remove is not in the cache\n");
    return;
  }

  Hyperbolic_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

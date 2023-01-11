//
//  Quick demotion + lazy promoition v1
//
//  20% FIFO + ARC
//  insert to ARC when evicting from FIFO
//
//
//  QDLPv1.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo/ARC.h"
#include "../../include/libCacheSim/evictionAlgo/priv/priv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *fifo1_head;
  cache_obj_t *fifo1_tail;
  // cache_obj_t *fifo2_head;
  // cache_obj_t *fifo2_tail;
  cache_t *main_cache;

  int64_t n_fifo1_obj;
  // int64_t n_fifo2_obj;
  int64_t n_fifo1_byte;
  // int64_t n_fifo2_byte;

  int64_t fifo1_max_size;
  int64_t main_cache_max_size;

} QDLPv1_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

void QDLPv1_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *QDLPv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("QDLPv1", ccache_params);
  cache->cache_init = QDLPv1_init;
  cache->cache_free = QDLPv1_free;
  cache->get = QDLPv1_get;
  cache->check = QDLPv1_check;
  cache->insert = QDLPv1_insert;
  cache->evict = QDLPv1_evict;
  cache->remove = QDLPv1_remove;
  cache->to_evict = QDLPv1_to_evict;
  cache->init_params = cache_specific_params;
  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(QDLPv1_params_t));
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  params->fifo1_head = NULL;
  params->fifo1_tail = NULL;
  params->n_fifo1_obj = 0;
  params->n_fifo1_byte = 0;
  params->fifo1_max_size = (int64_t)ccache_params.cache_size * 0.25;
  params->main_cache_max_size =
      ccache_params.cache_size - params->fifo1_max_size;

  common_cache_params_t ccache_params_arc = ccache_params;
  ccache_params_arc.cache_size = params->main_cache_max_size;
  params->main_cache = ARC_init(ccache_params_arc, NULL);

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv1-%.2lf", 0.25);

  return cache;
}

bool QDLPv1_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
bool QDLPv1_check(cache_t *cache, const request_t *req,
                  const bool update_cache) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  cache_obj_t *cached_obj = NULL;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cached_obj);

  if (cached_obj != NULL) {
    cached_obj->misc.freq += 1;
    cached_obj->misc.last_access_rtime = req->clock_time;
    cached_obj->misc.last_access_vtime = cache->n_req;
    cached_obj->next_access_vtime = req->next_access_vtime;
  } else {
    cache_hit = params->main_cache->check(params->main_cache, req, false);
    if (cache_hit) {
      params->main_cache->check(params->main_cache, req, update_cache);
    }
  }

  return cache_hit;
}

cache_obj_t *QDLPv1_insert(cache_t *cache, const request_t *req) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->fifo1_head, &params->fifo1_tail, obj);
  params->n_fifo1_obj += 1;
  params->n_fifo1_byte += req->obj_size + cache->obj_md_size;

  obj->misc.freq = 0;
  obj->misc.q_id = 1;

  return obj;
}

cache_obj_t *QDLPv1_to_evict(cache_t *cache) {
  assert(false);
  return NULL;
}

void QDLPv1_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  cache_obj_t *obj = params->fifo1_tail;
  params->fifo1_tail = obj->queue.prev;
  if (params->fifo1_tail != NULL) {
    params->fifo1_tail->queue.next = NULL;
  } else {
    params->fifo1_head = NULL;
  }
  DEBUG_ASSERT(obj->misc.q_id == 1);
  params->n_fifo1_obj -= 1;
  params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;

  if (obj->misc.freq > 0) {
    // promote to main cache
    copy_cache_obj_to_request(req_local, obj);
    DEBUG_ASSERT(params->main_cache->check(params->main_cache, req_local,
                                           false) == false);
    params->main_cache->get(params->main_cache, req_local);
    hashtable_delete(cache->hashtable, obj);  // remove from fifo1
    cache->occupied_byte =
        params->n_fifo1_byte + params->main_cache->occupied_byte;
    cache->n_obj = params->n_fifo1_obj + params->main_cache->n_obj;
  } else {
    // evict from fifo1
    cache_evict_base(cache, obj, true);
  }
}

void QDLPv1_remove_obj(cache_t *cache, cache_obj_t *obj) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  if (obj->misc.q_id == 1) {
    params->n_fifo1_obj -= 1;
    params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo1_head, &params->fifo1_tail, obj);
    cache_remove_obj_base(cache, obj, true);
  } else {
    bool removed = params->main_cache->remove(params->main_cache, obj->obj_id);
    assert(removed == true);
    cache_remove_obj_base(cache, obj, false);
  }
}

bool QDLPv1_remove(cache_t *cache, const obj_id_t obj_id) {
  QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    bool removed = params->main_cache->remove(params->main_cache, obj->obj_id);
    if (removed) {
      cache_remove_obj_base(cache, obj, false);
    }
    return removed;
  } else {
    assert(obj->misc.q_id == 1);
    params->n_fifo1_obj -= 1;
    params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo1_head, &params->fifo1_tail, obj);
    cache_remove_obj_base(cache, obj, true);
    return true;
  }
}

#ifdef __cplusplus
}
#endif

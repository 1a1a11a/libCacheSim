//
//  Quick demotion + lazy promoition v2
//
//  20% FIFO + 80% 2-bit clock, promote when evicting from FIFO
//
//
//  QDLPv2.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo/priv/priv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *fifo1_head;
  cache_obj_t *fifo1_tail;
  cache_obj_t *fifo2_head;
  cache_obj_t *fifo2_tail;

  int64_t n_fifo1_obj;
  int64_t n_fifo2_obj;
  int64_t n_fifo1_byte;
  int64_t n_fifo2_byte;

  int64_t fifo1_max_size;
  int64_t fifo2_max_size;

} QDLPv2_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

void QDLPv2_free(cache_t *cache) { cache_struct_free(cache); }

cache_t *QDLPv2_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("QDLPv2", ccache_params);
  cache->cache_init = QDLPv2_init;
  cache->cache_free = QDLPv2_free;
  cache->get = QDLPv2_get;
  cache->check = QDLPv2_check;
  cache->insert = QDLPv2_insert;
  cache->evict = QDLPv2_evict;
  cache->remove = QDLPv2_remove;
  cache->to_evict = QDLPv2_to_evict;
  cache->init_params = cache_specific_params;
  cache->obj_md_size = 0;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = malloc(sizeof(QDLPv2_params_t));
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;
  params->fifo1_head = NULL;
  params->fifo1_tail = NULL;
  params->fifo2_head = NULL;
  params->fifo2_tail = NULL;
  params->n_fifo1_obj = 0;
  params->n_fifo2_obj = 0;
  params->n_fifo1_byte = 0;
  params->n_fifo2_byte = 0;
  params->fifo1_max_size = (int64_t)ccache_params.cache_size * 0.25;
  params->fifo2_max_size = ccache_params.cache_size - params->fifo1_max_size;

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv2-%.2lf", 0.25);
  return cache;
}

bool QDLPv2_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
bool QDLPv2_check(cache_t *cache, const request_t *req,
                  const bool update_cache) {
  cache_obj_t *cached_obj = NULL;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cached_obj);
  if (cached_obj != NULL) {
    if (cached_obj->misc.freq < 4)
        cached_obj->misc.freq += 1;
  }

  return cache_hit;
}

cache_obj_t *QDLPv2_insert(cache_t *cache, const request_t *req) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->fifo1_head, &params->fifo1_tail, obj);
  params->n_fifo1_obj += 1;
  params->n_fifo1_byte += req->obj_size + cache->obj_md_size;

  obj->misc.freq = 0;
  obj->misc.q_id = 1;

  return obj;
}

cache_obj_t *QDLPv2_to_evict(cache_t *cache) {
  assert(false);
  return NULL;
}

void QDLPv2_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

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
    // promote to clock
    obj->misc.q_id = 2;
    obj->misc.freq -= 1;
    params->n_fifo2_obj += 1;
    params->n_fifo2_byte += obj->obj_size + cache->obj_md_size;
    prepend_obj_to_head(&params->fifo2_head, &params->fifo2_tail, obj);
    while (params->n_fifo2_byte > params->fifo2_max_size) {
      cache_obj_t *obj_to_evict = params->fifo2_tail;
      if (obj_to_evict->misc.freq > 0) {
        obj_to_evict->misc.freq -= 1;
        move_obj_to_head(&params->fifo2_head, &params->fifo2_tail,
                         obj_to_evict);
        continue;
      } else {
        remove_obj_from_list(&params->fifo2_head, &params->fifo2_tail,
                             obj_to_evict);
        params->n_fifo2_obj -= 1;
        params->n_fifo2_byte -= obj_to_evict->obj_size + cache->obj_md_size;
        cache_evict_base(cache, obj_to_evict, true);
      }
    }
  } else {
    // evict from fifo1
    cache_evict_base(cache, obj, true);
  }
}

void QDLPv2_remove_obj(cache_t *cache, cache_obj_t *obj) {
  QDLPv2_params_t *params = (QDLPv2_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  if (obj->misc.q_id == 1) {
    params->n_fifo1_obj -= 1;
    params->n_fifo1_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo1_head, &params->fifo1_tail, obj);
  } else if (obj->misc.q_id == 2) {
    params->n_fifo2_obj -= 1;
    params->n_fifo2_byte -= obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->fifo2_head, &params->fifo2_tail, obj);
  }

  cache_remove_obj_base(cache, obj, true);
}

bool QDLPv2_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  QDLPv2_remove_obj(cache, obj);

  return true;
}

#ifdef __cplusplus
}
#endif

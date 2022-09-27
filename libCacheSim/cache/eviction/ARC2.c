//
//  ARC2 cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#include "../include/libCacheSim/evictionAlgo/ARC2.h"

#include "../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

static void verify_lru(cache_obj_t *head, cache_obj_t *tail, int64_t n_obj) {
  int64_t n = 1;

  if (head == NULL) {
    assert(tail == NULL);
    assert(n_obj == 0);
    return;
  }

  cache_obj_t *curr = head;
  while (n < n_obj && curr != tail) {
    curr = curr->queue.next;
    n++;
  }

  if (curr != tail || n != n_obj) abort();
}

cache_t *ARC2_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("ARC2", ccache_params);
  cache->cache_init = ARC2_init;
  cache->cache_free = ARC2_free;
  cache->get = ARC2_get;
  cache->check = ARC2_check;
  cache->insert = ARC2_insert;
  cache->evict = ARC2_evict;
  cache->remove = ARC2_remove;
  cache->to_evict = ARC2_to_evict;

  cache->init_params = cache_specific_params;

  cache->eviction_params = my_malloc_n(ARC2_params_t, 1);
  memset(cache->eviction_params, 0, sizeof(ARC2_params_t));
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);
  params->evict_source = 1;

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  // if (init_params != NULL)
  //   params->ghost_list_size =
  //       ccache_params.cache_size * init_params->ghost_list_size_ratio / 2;
  // else
  params->ghost_list_size = ccache_params.cache_size / 2;

  return cache;
}

void ARC2_free(cache_t *cache) {
  ARC2_params_t *ARC2_params = (ARC2_params_t *)(cache->eviction_params);
  my_free(sizeof(ARC2_params_t), ARC2_params);
  cache_struct_free(cache);
}

cache_ck_res_e ARC2_check(cache_t *cache, const request_t *req,
                          const bool update_cache) {
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);

  verify_lru(params->lru1_head, params->lru1_tail, params->lru1_n_obj);
  verify_lru(params->lru2_head, params->lru2_tail, params->lru2_n_obj);

  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, false, &cache_obj);

  if (cache_obj) {
    if (cache_obj->ARC2.ghost) {
      ret = cache_ck_miss;  // ghost object
      if (cache_obj->ARC2.lru_id == 1) {
        params->evict_source = 2;
        // remove ghost object
        params->ghost_list1_n_obj -= 1;
        params->ghost_list1_occupied_size -=
            cache_obj->obj_size + cache->per_obj_overhead;

        if (cache_obj == params->lru1_ghost_tail) {
          params->lru1_ghost_tail = cache_obj->queue.prev;
          if (!params->lru1_ghost_tail->ARC2.ghost)
            params->lru1_ghost_tail = NULL;
        }

        remove_obj_from_list(&params->lru1_head, &params->lru1_tail, cache_obj);

      } else if (cache_obj->ARC2.lru_id == 2) {
        params->evict_source = 1;
        // remove ghost object
        params->ghost_list2_n_obj -= 1;
        params->ghost_list2_occupied_size -=
            cache_obj->obj_size + cache->per_obj_overhead;

        if (cache_obj == params->lru2_ghost_tail) {
          params->lru2_ghost_tail = cache_obj->queue.prev;
          if (!params->lru2_ghost_tail->ARC2.ghost)
            params->lru2_ghost_tail = NULL;
        }

        remove_obj_from_list(&params->lru2_head, &params->lru2_tail, cache_obj);

      } else {
        ERROR("error: lru_id is not 1 or 2\n");
        abort();
      }

      hashtable_delete(cache->hashtable, cache_obj);

    } else if (likely(update_cache)) {
      if (unlikely(cache_obj->obj_size != req->obj_size)) {
        VVERBOSE("object size change from %u to %u\n", cache_obj->obj_size,
                 req->obj_size);
        cache->occupied_size -= cache_obj->obj_size;
        cache->occupied_size += req->obj_size;
        cache_obj->obj_size = req->obj_size;
      }

      if (cache_obj->ARC2.lru_id == 1) {
        // move from LRU1 to LRU2 head
        printf("%ld move %ld to LRU2 - n_obj %lu\n", cache->n_req, req->obj_id,
               params->lru2_n_obj);
        remove_obj_from_list(&params->lru1_head, &params->lru1_tail, cache_obj);
        cache_obj->ARC2.lru_id = 2;

        if (unlikely(params->lru2_head == NULL)) {
          // an empty list, this is the first insert
          params->lru2_head = cache_obj;
          params->lru2_tail = cache_obj;
        } else {
          params->lru2_head->queue.prev = cache_obj;
          cache_obj->queue.next = params->lru2_head;
        }
        params->lru2_head = cache_obj;

        params->lru1_n_obj -= 1;
        params->lru2_n_obj += 1;

      } else if (cache_obj->ARC2.lru_id == 2) {
        // move to LRU2 head
        move_obj_to_head(&params->lru2_head, &params->lru2_tail, cache_obj);

      } else {
        ERROR("error: lru_id is not 1 or 2\n");
        abort();
      }
    }
  }

  verify_lru(params->lru1_head, params->lru1_tail, params->lru1_n_obj);
  verify_lru(params->lru2_head, params->lru2_tail, params->lru2_n_obj);

  return ret;
}

cache_ck_res_e ARC2_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

void ARC2_insert(cache_t *cache, const request_t *req) {
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);

  printf("%ld insert %ld to LRU1 - n_obj %lu\n", cache->n_req, req->obj_id,
         params->lru1_n_obj);

  /* first time add, then it should be add to LRU1 */
  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->ARC2.lru_id = 1;
  cache_obj->magic = 1248;

  cache_obj->queue.prev = NULL;
  if (unlikely(params->lru1_head == NULL)) {
    // an empty list, this is the first insert
    params->lru1_head = cache_obj;
    params->lru1_tail = cache_obj;
  } else {
    params->lru1_head->queue.prev = cache_obj;
    cache_obj->queue.next = params->lru1_head;
  }
  params->lru1_head = cache_obj;
  params->lru1_n_obj += 1;
}

cache_obj_t *ARC2_to_evict(cache_t *cache) {
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);

  if (params->evict_source == 1) {
    if (params->lru1_tail != NULL && !params->lru1_tail->ARC2.ghost)
      return params->lru1_tail;
    else
      return params->lru2_tail;
  } else if (params->evict_source == 2) {
    if (params->lru2_tail != NULL && !params->lru2_tail->ARC2.ghost)
      return params->lru2_tail;
    else
      return params->lru1_tail;
  } else {
    ERROR("error: evict_source is not 1 or 2\n");
    abort();
  }
}

void ARC2_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);
  cache_obj_t *obj_to_evict;
  int evict_source;

  verify_lru(params->lru1_head, params->lru1_tail, params->lru1_n_obj);
  verify_lru(params->lru2_head, params->lru2_tail, params->lru2_n_obj);

  if (params->evict_source == 1) {
    if (params->lru1_tail != NULL && !params->lru1_tail->ARC2.ghost) {
      /* evict from LRU 1 */
      evict_source = 1;
    } else {
      /* evict from LRU 2 */
      evict_source = 2;
    }
  } else if (params->evict_source == 2) {
    if (params->lru2_tail != NULL && !params->lru2_tail->ARC2.ghost) {
      /* evict from LRU 2 */
      evict_source = 2;
    } else {
      evict_source = 1;
      /* evict from LRU 1 */
    }
  } else {
    ERROR("error: evict_source is not 1 or 2\n");
    abort();
  }

  assert(params->lru1_tail != NULL || params->lru2_tail != NULL);
  if (evict_source == 1) {
    params->lru1_n_obj -= 1;

    obj_to_evict = params->lru1_tail;
    params->lru1_tail = params->lru1_tail->queue.prev;
    if (params->lru1_tail == NULL) params->lru1_head = NULL;
    DEBUG_ASSERT(obj_to_evict->ARC2.lru_id == 1);

    params->ghost_list1_occupied_size +=
        obj_to_evict->obj_size + cache->per_obj_overhead;
    params->ghost_list1_n_obj += 1;
    if (params->lru1_ghost_tail == NULL) params->lru1_ghost_tail = obj_to_evict;
    printf("%ld size %ld-%ld evict lru1 %lu - %lu\n", cache->n_req,
           cache->occupied_size, cache->cache_size, params->lru1_n_obj,
           params->ghost_list1_n_obj);

    while (params->ghost_list1_occupied_size > params->ghost_list_size) {
      // evict from ghost list
      printf("%ld evict lru1 ghost %lu\n", cache->n_req,
             params->ghost_list1_n_obj);

      cache_obj_t *ghost_to_evict = params->lru1_ghost_tail;
      DEBUG_ASSERT(ghost_to_evict != NULL && ghost_to_evict->ARC2.ghost);
      params->ghost_list1_occupied_size -=
          ghost_to_evict->obj_size + cache->per_obj_overhead;
      params->ghost_list1_n_obj -= 1;
      params->lru1_ghost_tail = ghost_to_evict->queue.prev;
      hashtable_delete(cache->hashtable, ghost_to_evict);
    }
  } else {
    obj_to_evict = params->lru2_tail;
    params->lru2_n_obj -= 1;

    params->lru2_tail = params->lru2_tail->queue.prev;
    if (params->lru2_tail == NULL) params->lru2_head = NULL;
    DEBUG_ASSERT(obj_to_evict->ARC2.lru_id == 2);

    params->ghost_list2_occupied_size +=
        obj_to_evict->obj_size + cache->per_obj_overhead;
    params->ghost_list2_n_obj += 1;
    if (params->lru2_ghost_tail == NULL) params->lru2_ghost_tail = obj_to_evict;
    printf("%ld size %ld-%ld evict lru2 %lu - %lu\n", cache->n_req,
           cache->occupied_size, cache->cache_size, params->lru2_n_obj,
           params->ghost_list2_n_obj);

    while (params->ghost_list2_occupied_size > params->ghost_list_size) {
      // evict from ghost list
      printf("%ld evict lru2 ghost %lu\n", cache->n_req,
             params->ghost_list2_n_obj);

      cache_obj_t *ghost_to_evict = params->lru2_ghost_tail;
      DEBUG_ASSERT(ghost_to_evict != NULL && ghost_to_evict->ARC2.ghost);
      params->ghost_list2_occupied_size -=
          ghost_to_evict->obj_size + cache->per_obj_overhead;
      params->ghost_list2_n_obj -= 1;
      params->lru2_ghost_tail = ghost_to_evict->queue.prev;
      hashtable_delete(cache->hashtable, ghost_to_evict);
    }
  }

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  obj_to_evict->ARC2.ghost = true;
  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  verify_lru(params->lru2_head, params->lru2_tail, params->lru2_n_obj);
  verify_lru(params->lru2_head, params->lru2_tail, params->lru2_n_obj);
}

void ARC2_remove(cache_t *cache, const obj_id_t obj_id) {
  ARC2_params_t *params = (ARC2_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);

  if (obj != NULL) {
    if (!obj->ARC2.ghost) {
      cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
      cache->n_obj -= 1;
    }

    if (obj->ARC2.lru_id == 1) {
      remove_obj_from_list(&params->lru1_head, &params->lru1_tail, obj);
    } else {
      remove_obj_from_list(&params->lru1_head, &params->lru1_tail, obj);
    }

    hashtable_delete(cache->hashtable, obj);
  }
}

#ifdef __cplusplus
}
#endif

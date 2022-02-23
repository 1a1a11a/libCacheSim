//
//  FIFOMerge scans N objects and retains M objects, 
//  it is similar to multi-bit clock, but it does bounded work, guarantees some objects are evicted, some are retained   
//
//
//  FIFOMerge.c
//  libCacheSim
//
//  Created by Juncheng on 12/20/21.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../include/libCacheSim/evictionAlgo/FIFOMerge.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

cache_t *FIFOMerge_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("FIFOMerge", ccache_params);

  FIFOMerge_params_t *params = my_malloc(FIFOMerge_params_t);
  if (init_params != NULL) {
    FIFOMerge_init_params_t *init_params_ptr = (FIFOMerge_init_params_t *)init_params;
    params->n_keep_obj = init_params_ptr->n_keep_obj;
    params->n_merge_obj = init_params_ptr->n_merge_obj;
    params->use_oracle = init_params_ptr->use_oracle;
  } else {
    // params->n_merge_obj = -1; // make this adaptive to cache size and object size 

    /* can we make this parameter learned? */ 
    params->n_merge_obj = 100;
    params->metric_list = my_malloc_n(struct fifo_merge_sort_list_node, params->n_merge_obj);
    params->n_keep_obj = params->n_merge_obj / 4;
    params->use_oracle = false;
  }

  if (params->use_oracle)
    memcpy(cache->cache_name, "FIFOMerge-optimal", strlen("FIFOMerge-optimal"));

  // how many to keep should be a parameter of miss ratio 
  // printf("size %lu MiB, n_merge_obj: %d, n_keep_obj: %d\n", 
  //         ccache_params.cache_size / 1024 / 1024, params->n_merge_obj, params->n_keep_obj);

  params->next_to_merge = NULL;
  cache->eviction_params = params;

  cache->cache_init = FIFOMerge_init;
  cache->cache_free = FIFOMerge_free;
  cache->get = FIFOMerge_get;
  cache->check = FIFOMerge_check;
  cache->insert = FIFOMerge_insert;
  cache->evict = FIFOMerge_evict;
  cache->remove = FIFOMerge_remove;
  cache->to_evict = FIFOMerge_to_evict;

  return cache;
}

void FIFOMerge_free(cache_t *cache) { 
  FIFOMerge_params_t *params = (FIFOMerge_params_t *)cache->eviction_params;
  my_free(sizeof(struct fifo_merge_sort_list_node) * params->n_merge_obj, params->metric_list); 

  cache_struct_free(cache); 
}

cache_ck_res_e FIFOMerge_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj; 
  cache_ck_res_e res = cache_check_base(cache, req, update_cache, &cache_obj);

  if (res == cache_ck_hit) {
    cache_obj->FIFOMerge.freq++;
    cache_obj->FIFOMerge.last_access_vtime = cache->n_req;
    cache_obj->FIFOMerge.next_access_vtime = req->next_access_vtime; 
  }

  return res; 
}

cache_ck_res_e FIFOMerge_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}


double oracle_metric(cache_t *cache, cache_obj_t *cache_obj) {
  if (cache_obj->FIFOMerge.next_access_vtime == -1 || cache_obj->FIFOMerge.next_access_vtime == INT64_MAX)
    return -1;
  return (double) 1000000000.0 / (cache_obj->FIFOMerge.next_access_vtime - cache->n_req) / (double) cache_obj->obj_size; 
}

double freq_metric(cache_t *cache, cache_obj_t *cache_obj) {
  return (double) cache_obj->FIFOMerge.freq / (double) cache_obj->obj_size * 1000.0; 
}

double FIFOMerge_promote_metric(cache_t *cache, cache_obj_t *cache_obj) {
  FIFOMerge_params_t *params = (FIFOMerge_params_t *)cache->eviction_params;

  // if (cache_obj->FIFOMerge.freq > 0)
    // printf("%ld %.4lf %.4lf %ld\n", cache_obj->obj_id, freq_metric(cache, cache_obj), oracle_metric(cache, cache_obj), cache_obj->FIFOMerge.next_access_vtime);
  // if (cache_obj->FIFOMerge.next_access_vtime == -1 || cache_obj->FIFOMerge.next_access_vtime == INT64_MAX)
  //   return -1;

  if (params->use_oracle) 
    return oracle_metric(cache, cache_obj);
  else 
    return freq_metric(cache, cache_obj);
}

void FIFOMerge_insert(cache_t *cache, request_t *req) { 
  cache_obj_t *cache_obj = cache_insert_LRU(cache, req); 
  cache_obj->FIFOMerge.freq = 0;
  cache_obj->FIFOMerge.last_access_vtime = cache->n_req;
  cache_obj->FIFOMerge.next_access_vtime = req->next_access_vtime; 
}

int cmp_list_node(const void *a0, const void *b0) {
  struct fifo_merge_sort_list_node *a = (struct fifo_merge_sort_list_node *)a0;
  struct fifo_merge_sort_list_node *b = (struct fifo_merge_sort_list_node *)b0;

  if (a->metric > b->metric) {
    return 1; 
  } else if (a->metric < b->metric) {
    return -1; 
  } else {
    return 0; 
  }
} 

cache_obj_t *FIFOMerge_to_evict(cache_t *cache) {
  ERROR("Undefined! Multiple objs will be evicted\n");
}

void FIFOMerge_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  assert(evicted_obj == NULL);

  FIFOMerge_params_t *params = (FIFOMerge_params_t *) cache->eviction_params;
  if (params->next_to_merge == NULL) {
    params->next_to_merge = cache->q_head;
  } 

  if (params->n_merge_obj == -1) {
    params->n_merge_obj = cache->n_obj / 200000 + 8;
    params->n_keep_obj = params->n_merge_obj / 4;
    params->metric_list = my_malloc_n(struct fifo_merge_sort_list_node, params->n_merge_obj);
    printf("cache size %lu n_merge_obj: %d, n_keep_obj: %d\n", cache->cache_size, params->n_merge_obj, params->n_keep_obj);
  }

  // collect metric for n_merge obj, we will keep objects with larger metric 
  cache_obj_t *cache_obj = params->next_to_merge;
  if (cache_obj == NULL) {
    params->next_to_merge = cache->q_head;
  }

  int n_loop = 0;
  for (int i = 0; i < params->n_merge_obj; i++) {
    assert(cache_obj != NULL);
    struct fifo_merge_sort_list_node n = {FIFOMerge_promote_metric(cache, cache_obj), cache_obj}; 
    params->metric_list[i] = n;
    cache_obj = cache_obj->queue.next;
    if (cache_obj == NULL) {
      cache_obj = cache->q_head;
      i = 0; 
      if (n_loop++ > 2) {
        ERROR("FIFOMerge_evict: loop too many times\n");

        cache_obj = params->next_to_merge;
        if (cache_obj->queue.next != NULL) {
          cache_obj = cache_obj->queue.next;
        }

        FIFOMerge_remove_obj(cache, params->next_to_merge);
        return; 
      }
    }
  }
  params->next_to_merge = cache_obj;
  
  // sort metrics 
  qsort(params->metric_list, params->n_merge_obj, sizeof(struct fifo_merge_sort_list_node), cmp_list_node);
  // printf("%.4lf %.4lf %.4lf %.4lf\n", params->metric_list[0].metric, params->metric_list[1].metric, params->metric_list[2].metric, params->metric_list[3].metric);

  // remove objects 
  for (int i = 0; i < params->n_merge_obj - params->n_keep_obj; i++) {
    cache_obj = params->metric_list[i].cache_obj;
    FIFOMerge_remove_obj(cache, cache_obj);
  }

  for (int i = 0; i < params->n_keep_obj; i++) {
    params->metric_list[i].cache_obj->FIFOMerge.freq = (params->metric_list[i].cache_obj->FIFOMerge.freq + 1) / 2;
  }
}

void FIFOMerge_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  if (obj_to_remove == NULL) {
    ERROR("remove NULL from cache\n");
    abort();
  }

  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove);
}

void FIFOMerge_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    ERROR("remove object %" PRIu64 "that is not cached\n", obj_id);
    return;
  }
  FIFOMerge_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif

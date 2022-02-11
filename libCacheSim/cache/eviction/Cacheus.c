/* Cacheus: FAST'21 */

#include "../include/libCacheSim/evictionAlgo/Cacheus.h"
#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/LRU.h"
#include "../include/libCacheSim/evictionAlgo/SR_LRU.h"
#include "../include/libCacheSim/evictionAlgo/CR_LFU.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

cache_t *Cacheus_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("Cacheus", ccache_params);
  cache->cache_init = Cacheus_init;
  cache->cache_free = Cacheus_free;
  cache->get = Cacheus_get;
  cache->check = Cacheus_check;
  cache->insert = Cacheus_insert;
  cache->evict = Cacheus_evict;
  cache->remove = Cacheus_remove;
  cache->to_evict = Cacheus_to_evict;

  cache->eviction_params = my_malloc_n(Cacheus_params_t, 1);
  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  params->ghost_list_factor = 1; 

  params->update_interval = ccache_params.cache_size; // From paper
  params->lr = 0.001 + ((double) (next_rand() % 1000)) / 1000;  // learning rate chooses randomly between 10-3 & 1
  // LR will be reset after 10 consecutive decreases. Whether reset to the same val or diff value, the repo differs from their paper. I followed paper.
  params->lr_previous = 0;

  params->w_lru = params->w_lfu = 0.50;  // weights for LRU and LFU
  params->num_hit = 0;
  params->hit_rate_prev = 0;

  params->LRU = SR_LRU_init(ccache_params, NULL); 
  params->LFU = CR_LFU_init(ccache_params, NULL);  
  SR_LRU_params_t *SR_LRU_params = (SR_LRU_params_t *) (params->LRU->eviction_params);
  // When SR_LRU evicts, makes CR_LFU evicts as well.
  SR_LRU_params->other_cache = params->LFU;

  /* set ghost_list_factor to 2 can reduce miss ratio anomaly */
  ccache_params.cache_size = (uint64_t) (
      (double) ccache_params.cache_size / 2 * params->ghost_list_factor);

  params->LRU_g = LRU_init(ccache_params, NULL); // LRU_history
  params->LFU_g = LRU_init(ccache_params, NULL); // LFU_history
  return cache; 
}

void Cacheus_free(cache_t *cache) { 
  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(Cacheus_params_t), params);
  cache_struct_free(cache);
}

static void update_weight(cache_t *cache, request_t *req) {

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) + (ck_lfu_g == cache_ck_hit ? 1 : 0) <= 1);

  if (ck_lru_g == cache_ck_hit){
    params->w_lru = params->w_lru * exp(-params->lr);  // decrease weight_LRU
  } else if (ck_lfu_g == cache_ck_hit) {
    params->w_lfu = params->w_lfu * exp(-params->lr);  // decrease weight_LFU
  }
  // normalize
  params->w_lru = params->w_lru / (params->w_lru + params->w_lfu);
  params->w_lfu = 1 - params->w_lru;
}

static void update_lr(cache_t *cache, request_t *req) {

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  // Here: num_hit is the number of hits; reset to 0 when update_lr is called.
  double hit_rate_current = params->num_hit / params->update_interval;
  double delta_hit_rate = hit_rate_current - params->hit_rate_prev;
  double delta_lr = params->lr - params->lr_previous;

  params->lr_previous = params->lr; 
  params->hit_rate_prev = hit_rate_current;

  if (delta_lr != 0){
    int sign;
    // Intuition: If hit rate is decreasing (deltla hit rate < 0)
    // Learning rate is positive (delta_lr > 0)
    // sign = -1 => decrease learning rate; 
    if (delta_hit_rate / delta_lr > 0)
      sign = 1;
    else
      sign = -1;
    // If hit rate > learning rate
    // sign positive, increase learning rate;

    // Repo has this part: not included in the original paper.
    // if self.learning_rate >= 1:
        // self.learning_rate = 0.9
    // elif self.learning_rate <= 0.001:
        // self.learning_rate = 0.005
    if (params->lr + sign * abs(params->lr * delta_lr) > 0.001)
      params->lr = params->lr + sign * abs(params->lr * delta_lr);
    else
      params->lr = 0.0001;
    params->unlearn_count = 0;
  } 
  else {
    if (hit_rate_current == 0 || delta_hit_rate <= 0)
      params->unlearn_count += 1;
    else if (params->unlearn_count >= 10) {
      params->unlearn_count = 0;
      params->lr = 0.001 + ((double) (next_rand() % 1000)) / 1000;  // learning rate chooses randomly between 10-3 & 1
    }
  } 
  params->num_hit = 0;
}

// Update weight and history, only called in cache miss.
static void check_and_update_history(cache_t *cache, request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);

  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) + (ck_lfu_g == cache_ck_hit ? 1 : 0) <= 1);

  update_weight(cache, req);

  if (ck_lru_g == cache_ck_hit) {
    cache_obj_t *obj = cache_get_obj(params->LRU_g, req);
    params->LRU_g->remove(params->LRU_g, req->obj_id);
  } else if (ck_lfu_g == cache_ck_hit) {
    cache_obj_t *obj = cache_get_obj(params->LFU_g, req);
    params->LFU_g->remove(params->LFU_g, req->obj_id);
  }
}

cache_ck_res_e Cacheus_check(cache_t *cache, request_t *req, bool update_cache) {

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  cache_ck_res_e ck_lru, ck_lfu;
  ck_lru = params->LRU->check(params->LRU, req, update_cache);
  ck_lfu = params->LFU->check(params->LFU, req, update_cache);
  DEBUG_ASSERT(ck_lru == ck_lfu);
  if (!update_cache) {
    return ck_lru;
  }

  if (ck_lru != cache_ck_hit) {
    /* cache miss */
    check_and_update_history(cache, req);
  }
  else {
    // If hit, update the data structure and increment num_hit counter
    params->num_hit += 1;
  }

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  cache->occupied_size = params->LRU->occupied_size;
  return ck_lru;
}

cache_ck_res_e Cacheus_get(cache_t *cache, request_t *req) {

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);
  cache->n_req ++;

  cache_ck_res_e ret = cache_get_base(cache, req);

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  if (cache->n_req % params->update_interval == 0)
    update_lr(cache, req);
  return ret;
}

void Cacheus_insert(cache_t *cache, request_t *req) {
  Cacheus_params_t *params= (Cacheus_params_t *) (cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  params->LRU->insert(params->LRU, req);
  params->LFU->insert(params->LFU, req);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj = params->LRU->n_obj;
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == params->LFU->n_obj);
}

cache_obj_t *Cacheus_to_evict(cache_t *cache) {

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  double r = ((double) (next_rand() % 100)) / 100.0;
  if (r < params->w_lru) { 
    return params->LRU->to_evict(params->LRU);
  } else {
    return params->LFU->to_evict(params->LFU);
  }
}

void Cacheus_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);
  cache_obj_t obj;
  double r = ((double) (next_rand() % 100)) / 100.0;

  // If two voters decide the same:
  if (params->LRU->to_evict(params->LRU)->obj_id == params->LFU->to_evict(params->LFU)->obj_id){
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->evict(params->LFU, req, &obj);
  }
  else if (r < params->w_lru) {
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->remove(params->LFU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LRU_g->check(params->LRU_g, req_local, false) == cache_ck_miss);
    params->LRU_g->insert(params->LRU_g, req_local);

  // action is LFU
  } else {
    params->LFU->evict(params->LFU, req, &obj);
    params->LRU->remove(params->LRU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LFU_g->check(params->LFU_g, req_local, false) == cache_ck_miss);
    params->LFU_g->insert(params->LFU_g, req_local);
  }

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &obj, sizeof(cache_obj_t));
  }

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj = params->LRU->n_obj;
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == params->LFU->n_obj);
}

void Cacheus_remove(cache_t *cache, obj_id_t obj_id) {
  Cacheus_params_t *params = (Cacheus_params_t *) (cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(params->LRU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %"PRIu64 "that is not cached in LRU\n", obj_id);
  }
  params->LRU->remove(params->LRU, obj_id);

  obj = cache_get_obj_by_id(params->LFU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %"PRIu64 "that is not cached in LFU\n", obj_id);
  }
  params->LFU->remove(params->LFU, obj_id);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}

#ifdef __cplusplus
}
#endif

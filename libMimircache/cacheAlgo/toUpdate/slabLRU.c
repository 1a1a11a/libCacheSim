//
// evict the least recent accessed slab
//
// Created by Juncheng Yang on 04/22/20.
//

#include "slabLRU.h"


#ifdef __cplusplus
extern "C" {
#endif


cache_t *slabLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("slabLRU", ccache_params);
  cache->cache_params = g_new0(slabLRU_params_t, 1);
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  slabLRU_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, slab_cache_obj_destroyer, g_free, slab_cache_obj_destroyer);
  slabLRU_params->slab_params.global_slab_q = g_queue_new();
  slabLRU_params->slab_params.slab_size = MB;
  slabLRU_params->slab_params.n_total_slabs = ccache_params.cache_size / slabLRU_params->slab_params.slab_size;
  slabLRU_params->slab_params.per_obj_metadata_size = 0;

  if (cache_specific_init_params != NULL){
    slab_init_params_t *init_params = cache_specific_init_params;
    cache->core.cache_specific_init_params = cache_specific_init_params;
    slabLRU_params->slab_params.slab_size = init_params->slab_size;
    slabLRU_params->slab_params.per_obj_metadata_size = init_params->per_obj_metadata_size;
  }

  for (int i=0; i<N_SLABCLASS; i++)
    init_slabclass(&slabLRU_params->slab_params, i);

  return cache;
}

void slabLRU_free(cache_t *cache){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  slab_params_t *slab_params = &slabLRU_params->slab_params;
  g_queue_free_full(slab_params->global_slab_q, _slab_destroyer);
  for (int i=0; i < N_SLABCLASS; i++){
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free(slabclass->obj_q);
  }

  g_hash_table_destroy(slabLRU_params->hashtable);
  cache_struct_free(cache);
}

gboolean slabLRU_check(cache_t *cache, request_t *req){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  return g_hash_table_contains(slabLRU_params->hashtable, req->obj_id_ptr);
}

gboolean slabLRU_get(cache_t *cache, request_t *req){
  gboolean found_in_cache = slabLRU_check(cache, req);

  /* no update, and eviction is done during insert */
  if (!found_in_cache)
    _slabLRU_insert(cache, req);
  else
    _slabLRU_update(cache, req);

  cache->core.req_cnt += 1;
  return found_in_cache;

}

void _slabLRU_insert(cache_t *cache, request_t *req){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  cache->core.used_size += req->obj_size;
  slab_cache_obj_t* cache_obj = create_slab_cache_obj_from_req(req);
  add_to_slabclass(cache, req, cache_obj, &slabLRU_params->slab_params, _slabLRU_evict, NULL);
  g_hash_table_insert(slabLRU_params->hashtable, cache_obj->obj_id_ptr, (gpointer) cache_obj);
}

void _slabLRU_update(cache_t *cache, request_t *req){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  slab_cache_obj_t *cache_obj = g_hash_table_lookup(slabLRU_params->hashtable, req->obj_id_ptr);
  slab_t *slab = (slab_t*) cache_obj->slab;
  g_queue_unlink(slabLRU_params->slab_params.global_slab_q, slab->q_node);
  g_queue_push_tail_link(slabLRU_params->slab_params.global_slab_q, slab->q_node);
}


void _slabLRU_evict(cache_t *cache, request_t *req){
  // evict the most recent accessed slab
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  slabLRU_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict = g_queue_pop_head(slabLRU_params->slab_params.global_slab_q);
  slabclass_t *slabclass = &slabLRU_params->slab_params.slabclasses[slab_to_evict->slabclass_id];

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  for (guint32 i=0; i<slab_to_evict->n_stored_items; i++){
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRU_params->hashtable, slab_to_evict->slab_items[i]);
    g_hash_table_remove(slabLRU_params->hashtable, cache_obj->obj_id_ptr);
//    cache_obj_destroyer((gpointer)cache_obj);
  }
  _slab_destroyer(slab_to_evict);
}

gpointer slabLRU_evict_with_return(cache_t *cache, request_t *req){
  ERROR("not suppoerted\n");
  return NULL;
}


void slabLRU_remove_obj(cache_t *cache, void *obj_id_to_remove){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRU_params->hashtable, obj_id_to_remove);
  if (cache_obj == NULL) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }

  cache->core.used_size -= cache_obj->obj_size;
  slab_t *slab = cache_obj->slab;
  // move last item to this pos
  slab->slab_items[cache_obj->item_pos_in_slab] = slab->slab_items[slab->n_stored_items-1];
  // update item_pos_in_slab
  slab_cache_obj_t *last_cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRU_params->hashtable, slab->slab_items[cache_obj->item_pos_in_slab]);
  last_cache_obj->item_pos_in_slab = cache_obj->item_pos_in_slab;
  slab->n_stored_items -=1;

  slabclass_t *slabclass = &slabLRU_params->slab_params.slabclasses[find_slabclass_id(cache_obj->obj_size)];
  slabclass->n_stored_items -= 1;
  g_queue_push_tail(slabclass->free_slab_q, slab);

  destroy_cache_obj(cache_obj);

}

GHashTable *slabLRU_get_objmap(cache_t *cache){
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *) (cache->cache_params);
  return slabLRU_params->hashtable;
}







cache_check_result_t slabLRU_check_and_update_with_ttl(cache_t *cache, request_t* req){
  slabLRU_params_t *params = (slabLRU_params_t *) (cache->cache_params);
  cache_check_result_t result = cache_miss_e;
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(params->hashtable, req->obj_id_ptr);
  if (cache_obj != NULL) {
    if (cache_obj->exp_time < req->real_time) {
      /* obj is expired */
      result = expired_e;
      cache_obj->exp_time = req->real_time + req->ttl;
    } else {
      result = cache_hit_e;
    }
  }
  return result;
}

gboolean slabLRU_get_with_ttl(cache_t *cache, request_t *req){
  req->ttl == 0 && (req->ttl = cache->core.default_ttl);
  cache_check_result_t result = slabLRU_check_and_update_with_ttl(cache, req);
  gboolean found_in_cache = (result == cache_hit_e);

  /* no update, and eviction is done during insert */
  if (result == cache_miss_e){
    _slabLRU_insert(cache, req);
  } else {
    _slabLRU_update(cache, req);
  }

  cache->core.req_cnt += 1;
  return found_in_cache;

}


#ifdef __cplusplus
}
#endif

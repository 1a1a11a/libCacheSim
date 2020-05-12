//
// Created by Juncheng Yang on 12/10/19.
//

#include "slabObjLRU.h"

//
// Created by Juncheng Yang on 11/22/19.
//

#include "slabObjLRU.h"


#ifdef __cplusplus
extern "C" {
#endif


void _slabObjLRU_evict_slab(cache_t* cache);


cache_t *slabObjLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("slabObjLRU", ccache_params);
  cache->cache_params = g_new0(slabObjLRU_params_t, 1);
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  slabObjLRU_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, NULL, g_free, NULL);
  slabObjLRU_params->slab_params.slab_q = g_queue_new();
  slabObjLRU_params->slab_params.slab_size = MB;
  slabObjLRU_params->slab_params.n_total_slabs = ccache_params.cache_size / slabObjLRU_params->slab_params.slab_size;
  slabObjLRU_params->slab_params.per_obj_metadata_size = 0;
  for (int i=0; i<N_SLABCLASS; i++)
    init_slabclass(&slabObjLRU_params->slab_params, i);

  return cache;
}

void slabObjLRU_free(cache_t *cache){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  slab_params_t *slab_params = &slabObjLRU_params->slab_params;
  g_queue_free_full(slab_params->slab_q, _slab_destroyer);
  for (int i=0; i < N_SLABCLASS; i++){
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free_full(slabclass->obj_q, cache_obj_destroyer);
  }

  g_hash_table_destroy(slabObjLRU_params->hashtable);
  cache_struct_free(cache);
}

gboolean slabObjLRU_check(cache_t *cache, request_t *req){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  return g_hash_table_contains(slabObjLRU_params->hashtable, req->obj_id_ptr);
}

gboolean slabObjLRU_get(cache_t *cache, request_t *req){
  gboolean found_in_cache = slabObjLRU_check(cache, req);

  /* no update, and eviction is done during insert */
  if (!found_in_cache)
    _slabObjLRU_insert(cache, req);
  else
    _slabObjLRU_update(cache, req);

  cache->core.req_cnt += 1;
  return found_in_cache;

}

void _slabObjLRU_insert(cache_t *cache, request_t *req){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  cache->core.used_size += req->obj_size;
  slab_cache_obj_t* cache_obj = create_slab_cache_obj_from_req(req);
  gint slab_id = add_to_slabclass(cache, req, cache_obj, &slabObjLRU_params->slab_params, _slabObjLRU_evict, NULL);
  // update obj LRU queue
  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[slab_id];
  GList *q_node = g_list_alloc();
  q_node->data = cache_obj;
  g_queue_push_tail_link(slabclass->obj_q, q_node);
  g_hash_table_insert(slabObjLRU_params->hashtable, cache_obj->obj_id_ptr, (gpointer) q_node);
}

void _slabObjLRU_update(cache_t *cache, request_t *req){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  GList *node = g_hash_table_lookup(slabObjLRU_params->hashtable, req->obj_id_ptr);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t*) node->data;
  slab_t *slab = (slab_t*) cache_obj->slab;
  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[slab->slabclass_id];
  // update obj LRU queue
  GQueue *q = slabclass->obj_q;
  g_queue_unlink(q, node);
  g_queue_push_tail_link(q, node);

  // update slab LRU queue
  q = slabObjLRU_params->slab_params.slab_q;
  g_queue_unlink(q, slab->q_node);
  g_queue_push_tail_link(q, slab->q_node);
  slabclass->last_access_time = req->real_time;
}


void _slabObjLRU_evict(cache_t *cache, request_t *req){
  // evict the most recent accessed obj
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  gint slab_id = find_slabclass_id(req->obj_size);
  GQueue *q = slabObjLRU_params->slab_params.slabclasses[slab_id].obj_q;
//  DEBUG("evict item from slabclass id %d, queue len %u\n", slab_id, g_queue_get_length(q));
  if (g_queue_get_length(q) == 0){
    _slabObjLRU_evict_slab(cache);
    return;
  }

  slab_cache_obj_t *obj_to_evict = (slab_cache_obj_t*) g_queue_pop_head(q);
  slab_t *slab = (slab_t*) obj_to_evict->slab;
  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[slab_id];
  g_queue_push_tail(slabclass->free_slab_q, slab);
  slabclass->n_stored_items -= 1;
  slab->n_stored_items -= 1;

  g_hash_table_remove(slabObjLRU_params->hashtable, obj_to_evict->obj_id_ptr);
  cache_obj_destroyer((gpointer)obj_to_evict);
}

void _slabObjLRU_evict_slab(cache_t* cache){
  /* take the least recent used slab and assign to the given slabclass */
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
//  slabclass_t *slabclass = slabObjLRU_params->slab_params.slabclasses;
//  guint64 smallest_ts = G_MAXUINT64;
//  gint slab_id = 0;
//  for (int i=0; i< N_SLABCLASS; i++){
//    if (slabclass[i].last_access_time < smallest_ts && g_queue_get_length(slabclass[i].)){
//      smallest_ts = slabclass[i].last_access_time;
//      slab_id = i;
//    }
//  }

  slabObjLRU_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict = g_queue_pop_head(slabObjLRU_params->slab_params.slab_q);
  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[slab_to_evict->slabclass_id];

  for (guint32 i=0; i<slab_to_evict->n_stored_items; i++){
    GList *node = (GList *) g_hash_table_lookup(slabObjLRU_params->hashtable, slab_to_evict->slab_items[i]);
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)node->data;
    g_hash_table_remove(slabObjLRU_params->hashtable, cache_obj->obj_id_ptr);
    g_queue_delete_link(slabclass->obj_q, node);
    cache_obj_destroyer((gpointer)cache_obj);
  }

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  _slab_destroyer(slab_to_evict);
}


gpointer slabObjLRU_evict_with_return(cache_t *cache, request_t *req){
  ERROR("not suppoerted\n");
  return NULL;
}


void slabObjLRU_remove_obj(cache_t *cache, void *obj_id_to_remove){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabObjLRU_params->hashtable, obj_id_to_remove);
  if (cache_obj == NULL) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }

  cache->core.used_size -= cache_obj->obj_size;
  slab_t *slab = cache_obj->slab;
  // move last item to this pos
  slab->slab_items[cache_obj->item_pos_in_slab] = slab->slab_items[slab->n_stored_items-1];
  // update item_pos_in_slab
  slab_cache_obj_t *last_cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabObjLRU_params->hashtable, slab->slab_items[cache_obj->item_pos_in_slab]);
  last_cache_obj->item_pos_in_slab = cache_obj->item_pos_in_slab;
  slab->n_stored_items -=1;

  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[find_slabclass_id(cache_obj->obj_size)];
  slabclass->n_stored_items -= 1;
  g_queue_push_tail(slabclass->free_slab_q, slab);

  destroy_cache_obj(cache_obj);

}

GHashTable *slabObjLRU_get_objmap(cache_t *cache){
  slabObjLRU_params_t *slabObjLRU_params = (slabObjLRU_params_t *) (cache->cache_params);
  return slabObjLRU_params->hashtable;
}









#ifdef __cplusplus
}
#endif

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

void _slabObjLRU_evict_slab(cache_t *cache);

void check_slab_cache_obj_valid(gpointer key, gpointer value,
                                gpointer user_data) {
  slab_cache_obj_t *cache_obj = ((GList *)value)->data;
  if (cache_obj->obj_id_ptr != key) {
    ERROR("here1 %ld %ld\n", (long)key, (long)cache_obj->obj_id_ptr);
    abort();
  }

  slab_t *slab = cache_obj->slab;
  if (slab->slab_items[cache_obj->item_pos_in_slab] != key) {
    ERROR("here2 %ld %ld @ %ld\n", (long)key,
          (long)slab->slab_items[cache_obj->item_pos_in_slab],
          (long)cache_obj->item_pos_in_slab);
    abort();
  }
}

void _check_slab_id_valid_aux(gpointer data, gpointer user_data) {
  slab_t *slab = data;
  if (slab->slabclass_id != GPOINTER_TO_INT(user_data)) {
    ERROR("find inconsistency %d %d\n", slab->slabclass_id,
          GPOINTER_TO_INT(user_data));
    abort();
  }
}

void _check_slab_id_valid(cache_t *cache) {
  printf("req %ld check\n", cache->req_cnt);
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  for (int i = 0; i < N_SLABCLASS; i++) {
    g_queue_foreach(slabObjLRU_params->slab_params.slabclasses[i].slab_q,
                    _check_slab_id_valid_aux, GINT_TO_POINTER(i));
  }
}

cache_t *slabObjLRU_init(common_cache_params_t ccache_params,
                         void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("slabObjLRU", ccache_params);
  cache->cache_params = g_new0(slabObjLRU_params_t, 1);
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  slabObjLRU_params->hashtable = create_hash_table_with_obj_id_type(
      ccache_params.obj_id_type, NULL, NULL, g_free, NULL);
  slabObjLRU_params->slab_params.global_slab_q = g_queue_new();
  slabObjLRU_params->slab_params.slab_size = MB;
  slabObjLRU_params->slab_params.n_total_slabs =
      ccache_params.cache_size / slabObjLRU_params->slab_params.slab_size;
  slabObjLRU_params->slab_params.per_obj_metadata_size = 0;

  if (cache_specific_init_params != NULL) {
    slab_init_params_t *init_params = cache_specific_init_params;
    cache->cache_specific_init_params = cache_specific_init_params;
    slabObjLRU_params->slab_params.slab_size = init_params->slab_size;
    slabObjLRU_params->slab_params.per_obj_metadata_size =
        init_params->per_obj_metadata_size;
    slabObjLRU_params->slab_params.slab_move_strategy =
        init_params->slab_move_strategy;
  }

  for (int i = 0; i < N_SLABCLASS; i++)
    init_slabclass(&slabObjLRU_params->slab_params, i);

  return cache;
}

void slabObjLRU_free(cache_t *cache) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  slab_params_t *slab_params = &slabObjLRU_params->slab_params;
  g_queue_free_full(slab_params->global_slab_q, _slab_destroyer);
  for (int i = 0; i < N_SLABCLASS; i++) {
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free_full(slabclass->obj_q, slab_cache_obj_destroyer);
  }

  g_hash_table_destroy(slabObjLRU_params->hashtable);
  cache_struct_free(cache);
}

gboolean slabObjLRU_check(cache_t *cache, request_t *req) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  return g_hash_table_contains(slabObjLRU_params->hashtable, req->obj_id_ptr);
}

void slabObjLRU_move_slab(cache_t *cache, int from_slab_id, int to_slab_id) {
  VERBOSE("automove slab from %d to %d\n", from_slab_id, to_slab_id);
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  slab_params_t *slab_params = &(slabObjLRU_params->slab_params);
  // find slab to evict
  //  printf("%d slab\n",
  //  g_queue_get_length(slab_params->slabclasses[from_slab_id].slab_q));
  slab_t *slab_to_evict =
      g_queue_pop_head(slab_params->slabclasses[from_slab_id].slab_q);
  slabObjLRU_params->slab_params.n_allocated_slabs -= 1;
  if (slab_to_evict->slabclass_id != from_slab_id) {
    ERROR("slab to evict does not belong to the slabclass %d!=%d\n",
          slab_to_evict->slabclass_id, from_slab_id);
    abort();
  }
  slabclass_t *slabclass =
      &slab_params->slabclasses[slab_to_evict->slabclass_id];

  for (guint32 i = 0; i < slab_to_evict->n_stored_items; i++) {
    //    slab_cache_obj_t *last_obj = ((GList*)
    //    g_hash_table_lookup(slabObjLRU_params->hashtable,
    //    slab->slab_items[slab->n_stored_items-1]))->data;

    GList *node = (GList *)g_hash_table_lookup(slabObjLRU_params->hashtable,
                                               slab_to_evict->slab_items[i]);
    if (node == NULL) {
      ERROR("req_cnt %lld cannot find object %ld\n", (long long)cache->req_cnt,
            (long)slab_to_evict->slab_items[i]);
      abort();
    }
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)node->data;
    g_hash_table_remove(slabObjLRU_params->hashtable, cache_obj->obj_id_ptr);
    g_queue_delete_link(slabclass->obj_q, node);
    slab_cache_obj_destroyer((gpointer)cache_obj);
  }

  // if current slab is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  g_queue_remove(slabclass->slab_q, slab_to_evict);
  g_queue_remove(slab_params->global_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  _slab_destroyer(slab_to_evict);

  // now move the slab to new class
  allocate_slab(slab_params, to_slab_id);
}

void slabObjLRU_slab_automove(cache_t *cache, request_t *req) {
  slab_params_t *slab_params =
      &(((slabObjLRU_params_t *)(cache->cache_params))->slab_params);
  if (slab_params->slab_move_strategy == recency_t) {
    //    _check_slab_id_valid(cache);
    guint64 oldest_age = 0, youngest_age = ~0U;
    int oldest_slabclass_id = -1, youngest_slabclass_id = -1;
    for (int i = 0; i < N_SLABCLASS; i++) {
      if (slab_params->slabclasses[i].n_stored_items == 0)
        continue;
      //      slab_cache_obj_t *oldest_obj =
      //      g_queue_peek_tail(slab_params->slabclasses[i].obj_q);
      slab_cache_obj_t *oldest_obj =
          g_queue_peek_head(slab_params->slabclasses[i].obj_q);
      if (req->real_time < oldest_obj->access_time) {
        ERROR("cur time %ld - oldest obj access time %ld\n",
              (long)req->real_time, (long)oldest_obj->access_time);
        abort();
      }
      guint64 oldest_age_cur_slabclass =
          req->real_time - oldest_obj->access_time;
      if (oldest_age_cur_slabclass > oldest_age &&
          g_queue_get_length(slab_params->slabclasses[i].slab_q) > 2) {
        oldest_age = oldest_age_cur_slabclass;
        oldest_slabclass_id = i;
      }
      if (oldest_age_cur_slabclass < youngest_age &&
          slab_params->slabclasses[i].n_evictions > 0) {
        youngest_age = oldest_age_cur_slabclass;
        youngest_slabclass_id = i;
      }
      slab_params->slabclasses[i].n_evictions = 0;
    }
    //    _check_slab_id_valid(cache);
    VVERBOSE("slab automove - check: youngest age %llu slab %d, oldest age "
             "%llu slab %d\n",
             youngest_age, youngest_slabclass_id, oldest_age,
             oldest_slabclass_id);
    if (youngest_age < default_slab_automove_recency_ratio * oldest_age) {
      // move
      slabObjLRU_move_slab(cache, oldest_slabclass_id, youngest_slabclass_id);
    }
  } else if (slab_params->slab_move_strategy == eviction_rate_t) {
    ERROR("slab automove based on eviction rate is not supported\n");
    abort();
  }
}

gboolean slabObjLRU_get(cache_t *cache, request_t *req) {
  slab_params_t *slab_params =
      &(((slabObjLRU_params_t *)(cache->cache_params))->slab_params);
  static guint64 last_automove_time = 0;
  gboolean found_in_cache = slabObjLRU_check(cache, req);

  if (!found_in_cache)
    _slabObjLRU_insert(cache, req);
  else
    _slabObjLRU_update(cache, req);

  if (slab_params->slab_move_strategy != no_automove &&
      req->real_time != last_automove_time) {
    if (slab_params->n_allocated_slabs == slab_params->n_total_slabs) {
      slabObjLRU_slab_automove(cache, req);
    }
    last_automove_time = req->real_time;
  }

  //  printf("%d\n", cache->req_cnt);
  //  g_hash_table_foreach(((slabObjLRU_params_t *)
  //  cache->cache_params)->hashtable, check_slab_cache_obj_valid, NULL);
  //  _check_slab_id_valid(cache);
  cache->req_cnt += 1;
  return found_in_cache;
}

void _slabObjLRU_insert(cache_t *cache, request_t *req) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  cache->used_size += req->obj_size;
  slab_cache_obj_t *cache_obj = create_slab_cache_obj_from_req(req);
  gint slab_id =
      add_to_slabclass(cache, req, cache_obj, &slabObjLRU_params->slab_params,
                       _slabObjLRU_evict, NULL);
  // update obj LRU queue
  slabclass_t *slabclass = &slabObjLRU_params->slab_params.slabclasses[slab_id];
  GList *q_node = g_list_alloc();
  q_node->data = cache_obj;
  g_queue_push_tail_link(slabclass->obj_q, q_node);
  g_hash_table_insert(slabObjLRU_params->hashtable, cache_obj->obj_id_ptr,
                      (gpointer)q_node);
}

void _slabObjLRU_update(cache_t *cache, request_t *req) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  GList *node =
      g_hash_table_lookup(slabObjLRU_params->hashtable, req->obj_id_ptr);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)node->data;
#ifdef SUPPORT_SLAB_AUTOMOVE
  cache_obj->access_time = req->real_time;
#endif

  slab_t *slab = (slab_t *)cache_obj->slab;
  slabclass_t *slabclass =
      &slabObjLRU_params->slab_params.slabclasses[slab->slabclass_id];
  // update obj LRU queue
  GQueue *q = slabclass->obj_q;
  g_queue_unlink(q, node);
  g_queue_push_tail_link(q, node);

  // update slab LRU queue
  q = slabObjLRU_params->slab_params.global_slab_q;
  g_queue_unlink(q, slab->q_node);
  g_queue_push_tail_link(q, slab->q_node);
  //  slabclass->last_access_time = req->real_time;
}

void _slabObjLRU_evict(cache_t *cache, request_t *req) {
  // evict the most recent accessed obj
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  gint slabclass_id = find_slabclass_id(req->obj_size);
  slabObjLRU_params->slab_params.slabclasses[slabclass_id].n_evictions += 1;
  GQueue *q = slabObjLRU_params->slab_params.slabclasses[slabclass_id].obj_q;
  //  DEBUG("evict item from slabclass id %d, queue len %u\n", slabclass_id,
  //  g_queue_get_length(q));
  if (g_queue_get_length(q) == 0) {
    _slabObjLRU_evict_slab(cache);
    return;
  }

  //  slab_cache_obj_t *obj_to_evict = (slab_cache_obj_t*) g_queue_peek_head(q);
  slab_cache_obj_t *obj_to_evict = (slab_cache_obj_t *)g_queue_pop_head(q);
  slab_t *slab = (slab_t *)obj_to_evict->slab;
  //  slab_cache_obj_t *objt = ((GList*)
  //  (g_hash_table_lookup(slabObjLRU_params->hashtable,
  //  obj_to_evict->obj_id_ptr)))->data; DEBUG("evict obj %ld %ld - pos
  //  %ld/%ld/%ld  \n", (long) obj_to_evict->obj_id_ptr, objt->obj_id_ptr,
  //  obj_to_evict->item_pos_in_slab, slab->n_stored_items,
  //  slab->n_total_items); DEBUG("a %ld %ld\n", obj_to_evict->obj_id_ptr,
  //  obj_to_evict->item_pos_in_slab); DEBUG("b %ld %ld\n", objt->obj_id_ptr,
  //  objt->item_pos_in_slab);
  if (slab->slab_items[obj_to_evict->item_pos_in_slab] !=
      obj_to_evict->obj_id_ptr) {
    ERROR("found inconsistency, obj_to_evict does not match the content in "
          "slab %p %p\n",
          slab->slab_items[obj_to_evict->item_pos_in_slab],
          obj_to_evict->obj_id_ptr);
    abort();
  }

  // move last obj to this position
  if (obj_to_evict->item_pos_in_slab != slab->n_stored_items - 1) {
    slab_cache_obj_t *last_obj =
        ((GList *)g_hash_table_lookup(
             slabObjLRU_params->hashtable,
             slab->slab_items[slab->n_stored_items - 1]))
            ->data;
    if (last_obj == NULL) {
      ERROR("find inconsistency in hashtable\n");
      abort();
    }
    //  DEBUG("evictObj, old last obj id %ld %ld item_pos %ld\n",
    //  slab->slab_items[slab->n_stored_items-1], last_obj->obj_id_ptr,
    //  last_obj->item_pos_in_slab);
    last_obj->item_pos_in_slab = obj_to_evict->item_pos_in_slab;
    slab->slab_items[obj_to_evict->item_pos_in_slab] =
        slab->slab_items[slab->n_stored_items - 1];
  }

  slab->slab_items[slab->n_stored_items - 1] = NULL;
  slabclass_t *slabclass =
      &slabObjLRU_params->slab_params.slabclasses[slabclass_id];
  g_queue_push_tail(slabclass->free_slab_q, slab);
  slabclass->n_stored_items -= 1;
  slab->n_stored_items -= 1;
  //  DEBUG("evictObj, old pos obj id  %ld\n",
  //  slab->slab_items[obj_to_evict->item_pos_in_slab]); DEBUG("evictObj, last
  //  pos obj %ld\n", slab->slab_items[last_obj->item_pos_in_slab]);

  g_hash_table_remove(slabObjLRU_params->hashtable, obj_to_evict->obj_id_ptr);
  slab_cache_obj_destroyer((gpointer)obj_to_evict);

  //  g_queue_pop_head(q);
}

void _slabObjLRU_evict_slab(cache_t *cache) {
  /* take the least recent used slab and assign to the given slabclass */
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  //  slabclass_t *slabclass = slabObjLRU_params->slab_params.slabclasses;
  //  guint64 smallest_ts = G_MAXUINT64;
  //  gint slabclass_id = 0;
  //  for (int i=0; i< N_SLABCLASS; i++){
  //    if (slabclass[i].last_access_time < smallest_ts &&
  //    g_queue_get_length(slabclass[i].)){
  //      smallest_ts = slabclass[i].last_access_time;
  //      slabclass_id = i;
  //    }
  //  }

  slabObjLRU_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict =
      g_queue_pop_head(slabObjLRU_params->slab_params.global_slab_q);
  slabclass_t *slabclass =
      &slabObjLRU_params->slab_params.slabclasses[slab_to_evict->slabclass_id];
  VVERBOSE("evict slab (class %d), items on slab n_store/n_total %ld/%ld\n",
           slab_to_evict->slabclass_id, (long)slab_to_evict->n_stored_items,
           (long)slab_to_evict->n_total_items);

  for (guint32 i = 0; i < slab_to_evict->n_stored_items; i++) {
    GList *node = (GList *)g_hash_table_lookup(slabObjLRU_params->hashtable,
                                               slab_to_evict->slab_items[i]);
    if (node == NULL) {
      ERROR("req_cnt %lld cannot find object at %p \n",
            (long long)cache->req_cnt, slab_to_evict->slab_items[i]);
      abort();
    }
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)node->data;
    g_hash_table_remove(slabObjLRU_params->hashtable, cache_obj->obj_id_ptr);
    g_queue_delete_link(slabclass->obj_q, node);
    slab_cache_obj_destroyer((gpointer)cache_obj);
  }

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  g_queue_remove(slabclass->slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  _slab_destroyer(slab_to_evict);
}

gpointer slabObjLRU_evict_with_return(cache_t *cache, request_t *req) {
  ERROR("not suppoerted\n");
  return NULL;
}

void slabObjLRU_remove_obj(cache_t *cache, void *obj_id_to_remove) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)g_hash_table_lookup(
      slabObjLRU_params->hashtable, obj_id_to_remove);
  if (cache_obj == NULL) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }

  cache->used_size -= cache_obj->obj_size;
  slab_t *slab = cache_obj->slab;
  // move last item to this pos
  slab->slab_items[cache_obj->item_pos_in_slab] =
      slab->slab_items[slab->n_stored_items - 1];
  // update item_pos_in_slab
  slab_cache_obj_t *last_cache_obj = (slab_cache_obj_t *)g_hash_table_lookup(
      slabObjLRU_params->hashtable,
      slab->slab_items[cache_obj->item_pos_in_slab]);
  last_cache_obj->item_pos_in_slab = cache_obj->item_pos_in_slab;
  slab->n_stored_items -= 1;

  slabclass_t *slabclass =
      &slabObjLRU_params->slab_params
           .slabclasses[find_slabclass_id(cache_obj->obj_size)];
  slabclass->n_stored_items -= 1;
  g_queue_push_tail(slabclass->free_slab_q, slab);

  destroy_cache_obj(cache_obj);
}

GHashTable *slabObjLRU_get_objmap(cache_t *cache) {
  slabObjLRU_params_t *slabObjLRU_params =
      (slabObjLRU_params_t *)(cache->cache_params);
  return slabObjLRU_params->hashtable;
}

cache_check_result_t slabObjLRU_check_and_update_with_ttl(cache_t *cache,
                                                          request_t *req) {
  slabObjLRU_params_t *params = (slabObjLRU_params_t *)(cache->cache_params);
  cache_check_result_t result = cache_miss_e;
  GList *node =
      (GList *)g_hash_table_lookup(params->hashtable, req->obj_id_ptr);
  if (node != NULL) {
    slab_cache_obj_t *cache_obj = node->data;
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

gboolean slabObjLRU_get_with_ttl(cache_t *cache, request_t *req) {
  slab_params_t *slab_params =
      &(((slabObjLRU_params_t *)(cache->cache_params))->slab_params);
  static guint64 last_automove_time = 0;

  req->ttl == 0 && (req->ttl = cache->default_ttl);
  cache_check_result_t result =
      slabObjLRU_check_and_update_with_ttl(cache, req);
  gboolean found_in_cache = (result == cache_hit_e);

  if (result == cache_miss_e) {
    _slabObjLRU_insert(cache, req);
  } else {
    _slabObjLRU_update(cache, req);
  }

  if (req->real_time != last_automove_time) {
    if (slab_params->n_allocated_slabs >= slab_params->n_total_slabs) {
      slabObjLRU_slab_automove(cache, req);
    }
    last_automove_time = req->real_time;
  }

  //  printf("req %d %d\n", cache->req_cnt, found_in_cache);
  //  _check_slab_id_valid(cache);
  cache->req_cnt += 1;
  return found_in_cache;
}

#ifdef __cplusplus
}
#endif

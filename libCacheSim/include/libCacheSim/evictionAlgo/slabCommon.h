//
// Created by Juncheng Yang on 4/22/20.
//

#ifndef libCacheSim_SLABCOMMON_H
#define libCacheSim_SLABCOMMON_H

#include "../cacheObj.h"
#include "../request.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>

#ifdef __cplusplus
extern "C" {
#endif

#define N_SLABCLASS 48
static uint32_t SLABCLASS_SIZES[] = {
    96,     120,     152,     192,     240,     304,     384,     480,
    600,    752,     944,     1184,    1480,    1856,    2320,    2904,
    3632,   4544,    5680,    7104,    8880,    11104,   13880,   17352,
    21696,  27120,   33904,   42384,   52984,   66232,   82792,   103496,
    129376, 161720,  202152,  252696,  315872,  394840,  524288,  655360,
    819200, 1048576, 1280000, 1600000, 2097152, 2500000, 3125000, 3906250};
#define default_slab_automove_recency_ratio 0.8

typedef enum {
  no_automove = 0,
  recency_t = 1,
  eviction_rate_t,
} slab_automove_t;

typedef struct {
  int64_t slab_size;
  slab_automove_t slab_move_strategy;
  int8_t per_obj_metadata_size;
} slab_init_params_t;

typedef struct {
  gint slabclass_id;
  uint32_t n_total_items;
  uint32_t n_stored_items;
  GList *q_node; // the node in global slab queue, used for slabLRU
  gpointer *slab_items;
} slab_t;

typedef struct {
  uint32_t chunk_size;
  uint32_t n_stored_items;
  GQueue *free_slab_q;
  GQueue *slab_q;     // the slabs of this slabclass, used in slab automove
  GQueue *obj_q;      // used only for obj eviction algo
  int32_t n_evictions; // number of evictions since last automove, used for slab
                      // automove
  //  uint32_t last_insert_time;     // used for slab automove
} slabclass_t;

typedef struct {
  uint64_t slab_size;
  slabclass_t slabclasses[N_SLABCLASS];
  uint64_t n_total_slabs;
  uint64_t n_allocated_slabs;
  GQueue *global_slab_q;       /* slab queue for slabLRC and slabLRU */
  uint8_t per_obj_metadata_size; // memcached on 64-bit system has 48/56 byte
                               // metadata (no cas/cas)
  slab_automove_t slab_move_strategy;
} slab_params_t;

typedef void (*evict_func_ptr_t)(cache_t *, request_t *,
                                 cache_obj_t *cache_obj);

typedef void (*slab_move_func_ptr_t)(cache_t *, request_t *);

static gint find_slabclass_id(uint32_t obj_size);

static slab_t *allocate_slab(slab_params_t *slab_params, gint id);

static void _slab_destroyer(gpointer data) {
  slab_t *slab = (slab_t *)data;
  //  for (int i=0; i<slab->n_stored_items; i++)
  //    cache_obj_destroyer(slab->slab_items[i]);
  //  g_free(slab->slab_items);
  //  g_free(data);
  my_free(sizeof(slab_cache_obj_t) * slab->n_total_items, slab->slab_items);
  my_free(sizeof(slab_t), slab);
}

static inline void init_slabclass(slab_params_t *slab_params, gint id) {
  slab_params->slabclasses[id].chunk_size = SLABCLASS_SIZES[id];
  slab_params->slabclasses[id].n_stored_items = 0;
  slab_params->slabclasses[id].free_slab_q = g_queue_new();
  slab_params->slabclasses[id].obj_q = g_queue_new();
  slab_params->slabclasses[id].slab_q = g_queue_new();
  //  allocate_slab(slab_params, id);
}

static inline gboolean _add_to_slab(slab_t *slab, request_t *req,
                                    slab_cache_obj_t *cache_obj) {
  slab->slab_items[slab->n_stored_items++] =
      GSIZE_TO_POINTER(cache_obj->obj_id_int);
  return slab->n_total_items == slab->n_stored_items;
}

static inline slab_t *allocate_slab(slab_params_t *slab_params, gint id) {
  slab_t *new_slab = my_malloc(slab_t);
  memset(new_slab, 0, sizeof(slab_t));
  uint32_t chunk_size = SLABCLASS_SIZES[id];
  new_slab->slabclass_id = id;
  new_slab->n_stored_items = 0;
  new_slab->n_total_items = slab_params->slab_size / chunk_size;
  new_slab->slab_items = my_malloc_n(gpointer, new_slab->n_total_items);
  memset(new_slab->slab_items, 0, sizeof(gpointer) * new_slab->n_total_items);
  slabclass_t *slabclass = &slab_params->slabclasses[id];

  // store the slab in global slab queue for slab eviction
  GList *q_node = g_list_alloc();
  q_node->data = new_slab;
  new_slab->q_node = q_node;
  g_queue_push_tail_link(slab_params->global_slab_q, q_node);

  // store the slab in slabclass free queue
  //  printf("allocate to %d global %d\n", id,
  //  g_queue_get_length(slab_params->global_slab_q));
  g_queue_push_tail(slabclass->free_slab_q, new_slab);
  g_queue_push_tail(slabclass->slab_q, new_slab);

  slab_params->n_allocated_slabs += 1;
  VVERBOSE(
      "allocate slab (slabclass %d) allocated slab %ld/%ld - fit %ld objects\n",
      id, (long)slab_params->n_allocated_slabs,
      (long)slab_params->n_total_slabs, (long)new_slab->n_total_items);
  return new_slab;
}

/**
 * add an object into slab, if there is no available space, evict before
 * inserting it
 *
 * return slab_id
 * @param cache
 * @param req
 * @param cache_obj
 * @param slab_params
 * @param evict_func
 * @param slab_move_func
 * @return
 */
static inline gint add_to_slabclass(cache_t *cache, request_t *req,
                                    slab_cache_obj_t *cache_obj,
                                    slab_params_t *slab_params,
                                    evict_func_ptr_t evict_func,
                                    slab_move_func_ptr_t slab_move_func) {
  gint slab_id = find_slabclass_id(req->obj_size);
  slabclass_t *slabclass = &slab_params->slabclasses[slab_id];
  //  slabclass->last_insert_time = req->real_time;
  slab_t *slab = NULL;

  //  printf("%u allocated %u total\n", (unsigned)
  //  slab_params->n_allocated_slabs, (unsigned) slab_params->n_total_slabs);
  if ((slab = (slab_t*) g_queue_peek_head(slabclass->free_slab_q)) != NULL) {
    // we still have free space for items, no new slab is needed
    //    slab = (slab_t *) g_queue_peek_head(slabclass->free_slab_q);
    VVVERBOSE("find free slab %p, items %p\n", slab, slab->slab_items);
  } else if (slab_params->n_allocated_slabs < slab_params->n_total_slabs) {
    // all slabs are full, but we still have free space to allocate new slabs
    slab = allocate_slab(slab_params, slab_id);
    VVVERBOSE("allocate new slab %p, items %p\n", slab, slab->slab_items);
  }

  // store obj
  if (slab != NULL) {
    slabclass->n_stored_items += 1;
    cache_obj->slab = slab;
    cache_obj->item_pos_in_slab = slab->n_stored_items;
    slab->slab_items[slab->n_stored_items++] =
        GSIZE_TO_POINTER(cache_obj->obj_id_int);
    if (slab->n_total_items == slab->n_stored_items) {
      // current slab is full, remove it out of free_slab_q
      g_queue_pop_head(slabclass->free_slab_q);
    }
  } else {
    //    DEBUG("%u/%u slabs\n", (unsigned) slab_params->n_allocated_slabs,
    //    (unsigned) slab_params->n_total_slabs); DEBUG("add req %lld: slabclass
    //    %d, chunk size %lu - %lu total items - %lu stored items\n",
    //           (int64_t) req->obj_id_ptr, slab_id, (unsigned long)
    //           slabclass->chunk_size, (unsigned long)
    //           slabclass->n_stored_items, (unsigned long)
    //           slabclass->n_stored_items);
    // we do not have free space, so we need to evict item/slab
    evict_func(cache, req, NULL);
    if (slab_move_func != NULL) {
      slab_move_func(cache, req);
    }
    return add_to_slabclass(cache, req, cache_obj, slab_params, evict_func,
                            slab_move_func);
  }
  return slab_id;
}

static inline gint find_slabclass_id(uint32_t obj_size) {
  gint id = 0, imin, imax;

  /* binary search */
  imin = 0;
  imax = N_SLABCLASS;
  while (imax >= imin) {
    id = (imin + imax) / 2;
    if (obj_size > SLABCLASS_SIZES[id]) {
      imin = id + 1;
    } else if (id > 0 && obj_size <= SLABCLASS_SIZES[id - 1]) {
      imax = id - 1;
    } else {
      break;
    }
  }

  if (imin > imax) { /* size too big for any slab */
    ERROR("size %u too large to fit in slab\n", obj_size);
    abort();
  }

  return id;
}

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_SLABCOMMON_H

//
// Created by Juncheng Yang on 12/10/19.
//

#ifndef MIMIRCACHE_SLABOBJLRU_H
#define MIMIRCACHE_SLABOBJLRU_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"



typedef struct slab_class {
  uint32_t chunk_size;
  uint32_t num_total_chunk;
  uint32_t num_used_chunk;
  // need a lock here for parallel version

  // LRU structs
  GQueue *lru_list;
} slab_class_t;

typedef struct slab {

} slab_t;



typedef struct slabObjLRU_params {
  GHashTable *hashtable;
  GQueue *list;
    uint64_t ts;

    uint32_t slab_size;
  uint32_t num_slab;
  slab_class_t *slab_classes;
  slab_t *slabs;

  /* memcached uses 96, 120, 152, 192, 240, 304, 384, 480, 600, 752, 944, 1184, 1480, 1856, 2320,
   * 2904, 3632, 4544, 5680, 7104, 8880, 11104, 13880, 17352, 21696, 27120, 33904, 42384, 52984,
   * 66232, 82792, 103496, 129376, 161720, 202152, 252696, 315872, 394840, 524288 */
  uint32_t *slab_class_sizes;
  int8_t per_obj_metadata_size;  // memcached on 64-bit system has 48/56 byte metadata (no cas/cas)

} slabObjLRU_params_t;


extern gboolean slabObjLRU_check(cache_t *cache, request_t *req);

extern gboolean slabObjLRU_add(cache_t *cache, request_t *req);

extern void _slabObjLRU_insert(cache_t *slabObjLRU, request_t *req);

extern void _slabObjLRU_update(cache_t *slabObjLRU, request_t *req);

extern void _slabObjLRU_evict(cache_t *slabObjLRU, request_t *req);

extern void *_slabObjLRU_evict_with_return(cache_t *slabObjLRU, request_t *req);


extern void slabObjLRU_destroy(cache_t *cache);

extern void slabObjLRU_destroy_unique(cache_t *cache);


cache_t *slabObjLRU_init(guint64 size, obj_id_type_t obj_id_type, void *params);


extern void slabObjLRU_remove_obj(cache_t *cache, void *data_to_remove);

extern guint64 slabObjLRU_get_current_size(cache_t *cache);


#ifdef __cplusplus
}
#endif


#endif //MIMIRCACHE_SLABOBJLRU_H

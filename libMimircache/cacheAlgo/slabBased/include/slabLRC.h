//
// Created by Juncheng Yang on 11/22/19.
//

#ifndef MIMIRCACHE_SLABLRC_H
#define MIMIRCACHE_SLABLRC_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/mimircache/cache.h"
#include "utilsInternal.h"


typedef struct slab_class {
  uint32_t chunk_size;
  // a LRU struct here
} slab_class_t;

typedef struct slab {

} slab_t;


typedef struct slabLRC_params {
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

} slabLRC_params_t;


extern gboolean slabLRC_check(cache_t *cache, request_t *req);

extern gboolean slabLRC_add(cache_t *cache, request_t *req);

extern void _slabLRC_insert(cache_t *slabLRC, request_t *req);

extern void _slabLRC_update(cache_t *slabLRC, request_t *req);

extern void _slabLRC_evict(cache_t *slabLRC, request_t *req);

extern void *_slabLRC_evict_with_return(cache_t *slabLRC, request_t *req);


extern void slabLRC_destroy(cache_t *cache);

extern void slabLRC_destroy_unique(cache_t *cache);


cache_t *slabLRC_init(guint64 size, obj_id_t obj_id_type, void *params);


extern void slabLRC_remove_obj(cache_t *cache, void *data_to_remove);

extern guint64 slabLRC_get_current_size(cache_t *cache);


#ifdef __cplusplus
}
#endif

#endif //MIMIRCACHE_SLABLRC_H

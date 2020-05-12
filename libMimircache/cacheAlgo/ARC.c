//
//  ARC.h
//  libMimircache
//
//  Created by Juncheng on 2/12/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include "ARC.h"
#include "../../include/mimircache/cacheOp.h"


void _ARC_insert(cache_t *cache, request_t *cp) {
  /* first time add, then it should be add to LRU1 */
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);

#ifdef SANITY_CHECK
  if (ARC_params->LRU1->core.check(ARC_params->LRU1, cp)) {
    fprintf(stderr, "ERROR: in ARC insert, inserted item is in cacheAlgo\n");
    exit(1);
  }
#endif

  ARC_params->LRU1->core._insert(ARC_params->LRU1, cp);
  ARC_params->size1++;
}

gboolean ARC_check(cache_t *cache, request_t *cp) {
  /* check both segments */
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  return (ARC_params->LRU1->core.check(ARC_params->LRU1, cp) ||
          ARC_params->LRU2->core.check(ARC_params->LRU2, cp));
}

void _ARC_update(cache_t *cache, request_t *cp) {
  /* if in seg1, then move to seg2;
   * if in seg2, just update
   */
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  if (ARC_params->LRU1->core.check(ARC_params->LRU1, cp)) {
    ARC_params->LRU1->core.remove_obj(ARC_params->LRU1, cp->obj_id_ptr);
    ARC_params->LRU2->core._insert(ARC_params->LRU2, cp);
    ARC_params->size1--;
    ARC_params->size2++;
  } else {
#ifdef SANITY_CHECK
    if (!ARC_params->LRU2->core.check(ARC_params->LRU2, cp)) {
      fprintf(stderr, "ERROR: in ARC insert, update element in LRU2, but"
                      " item is not in cacheAlgo\n");
      exit(1);
    }
#endif
    ARC_params->LRU2->core._update(ARC_params->LRU2, cp);
  }
}

void _ARC_evict(cache_t *cache, request_t *cp) {
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);

  /* check whether it is a hit on LRU1g or LRU2g,
   * if yes, evcit from the other LRU segments
   * if no,  evict from LRU1.
   * after evicting, needs to add to corresponding ghost list,
   * if ghost list is too large, then evict from ghost list
   */
  gpointer evicted_item, old_cp_itemp;
  cache_t *evicted_from, *remove_ghost, *add_ghost;
  gint64 *size;
  if (ARC_params->LRU1g->core.check(ARC_params->LRU1g, cp)) {
    // hit on LRU1g, evict from LRU2 and remove from LRU1g,
    // then add evicted to LRU2g
    if (ARC_params->size2 > 0) {
      evicted_from = ARC_params->LRU2;
      remove_ghost = ARC_params->LRU1g;
      add_ghost = ARC_params->LRU2g;
      size = &(ARC_params->size2);
    } else {
      evicted_from = ARC_params->LRU1;
      remove_ghost = ARC_params->LRU1g;
      add_ghost = ARC_params->LRU1g;
      size = &(ARC_params->size1);
    }
  } else if (ARC_params->LRU2g->core.check(ARC_params->LRU2g, cp)) {
    // hit on LRU2g, evict from LRU1 and remove from LRU2g,
    // then add evicted to LRU1g
    evicted_from = ARC_params->LRU1;
    remove_ghost = ARC_params->LRU2g;
    add_ghost = ARC_params->LRU1g;
    size = &(ARC_params->size1);
  } else {
    // not hit anywhere, evict from LRU1, then add evicted to LRU1g
    evicted_from = ARC_params->LRU1;
    remove_ghost = NULL;
    add_ghost = ARC_params->LRU1g;
    size = &(ARC_params->size1);
  }

  // now do the eviction, remove ghost, add ghost entry
  evicted_item = evicted_from->core.evict_with_return(evicted_from, cp);
  if (remove_ghost)
    remove_ghost->core.remove_obj(remove_ghost, cp->obj_id_ptr);
  (*size)--;
  old_cp_itemp = cp->obj_id_ptr;
  cp->obj_id_ptr = evicted_item;
#ifdef SANITY_CHECK
  if (add_ghost->core.check(add_ghost, cp)) {
    fprintf(stderr, "ERROR: in ARC evict, evicted element in ghost list\n");
    exit(1);
  }
#endif
  add_ghost->core.get(add_ghost, cp);
  cp->obj_id_ptr = old_cp_itemp;

  g_free(evicted_item);
}

gpointer _ARC_evict_with_return(cache_t *cache, request_t *cp) {
  /* evict one element and return the evicted element,
   * needs to free the memory of returned data later by user
   */

  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);

  gpointer evicted_item, old_cp_itemp;
  if (ARC_params->LRU1g->core.check(ARC_params->LRU1g, cp)) {
    // hit on LRU1g, evict from LRU2 and remove from LRU1g,
    // then add evicted to LRU2g
    evicted_item =
      ARC_params->LRU2->core.evict_with_return(ARC_params->LRU2, cp);
    ARC_params->LRU1g->core.remove_obj(ARC_params->LRU1g, cp->obj_id_ptr);
    ARC_params->size2--;
    old_cp_itemp = cp->obj_id_ptr;
    cp->obj_id_ptr = evicted_item;
    ARC_params->LRU2g->core.get(ARC_params->LRU2g, cp);
    cp->obj_id_ptr = old_cp_itemp;
  } else if (ARC_params->LRU2g->core.check(ARC_params->LRU2g, cp)) {
    // hit on LRU2g, evict from LRU1 and remove from LRU2g,
    // then add evicted to LRU1g
    evicted_item =
      ARC_params->LRU1->core.evict_with_return(ARC_params->LRU1, cp);
    ARC_params->LRU2g->core.remove_obj(ARC_params->LRU2g, cp->obj_id_ptr);
    ARC_params->size1--;
    old_cp_itemp = cp->obj_id_ptr;
    cp->obj_id_ptr = evicted_item;
    ARC_params->LRU1g->core.get(ARC_params->LRU1g, cp);
    cp->obj_id_ptr = old_cp_itemp;
  } else {
    // not hit anywhere, evict from LRU1, then add evicted to LRU1g
    evicted_item =
      ARC_params->LRU1->core.evict_with_return(ARC_params->LRU1, cp);
    ARC_params->size1--;
    old_cp_itemp = cp->obj_id_ptr;
    cp->obj_id_ptr = evicted_item;
    ARC_params->LRU1g->core.get(ARC_params->LRU1g, cp);
    cp->obj_id_ptr = old_cp_itemp;
  }
  return evicted_item;
}

gboolean ARC_add(cache_t *cache, request_t *cp) {
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  gboolean retval;

  if (ARC_check(cache, cp)) {
    _ARC_update(cache, cp);
    retval = TRUE;
  } else {
    _ARC_insert(cache, cp);
    if (ARC_params->size1 + ARC_params->size2 > cache->core.size)
      _ARC_evict(cache, cp);
    retval = FALSE;
#ifdef SANITY_CHECK
    if ((ARC_params->size1 + ARC_params->size2 > cacheAlgo->core.size) ||
        (ARC_params->LRU1->core.get_used_size(ARC_params->LRU1) +
         ARC_params->LRU2->core.get_used_size(ARC_params->LRU2)) >
            cacheAlgo->core.size) {
      fprintf(stderr,
              "ERROR: in ARC add, after inserting, "
              "sum of two LRUs sizes: %lu, ARC size1+size2=%lu, "
              "but cacheAlgo size=%ld\n",
              (unsigned long)(ARC_params->LRU1->core.get_used_size(
                                  ARC_params->LRU1) +
                              ARC_params->LRU2->core.get_used_size(
                                  ARC_params->LRU2)),
              (unsigned long)(ARC_params->size1 + ARC_params->size2),
              cacheAlgo->core.size);
      exit(1);
    }
#endif
  }

  cache->core.req_cnt += 1;
  return retval;
}

void ARC_destroy(cache_t *cache) {
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  ARC_params->LRU1->core.cache_free(ARC_params->LRU1);
  ARC_params->LRU1g->core.cache_free(ARC_params->LRU1g);
  ARC_params->LRU2->core.cache_free(ARC_params->LRU2);
  ARC_params->LRU2g->core.cache_free(ARC_params->LRU2g);
  cache_struct_free(cache);
}

//void ARC_destroy_unique(cache_t *cache) {
//  /* the difference between destroy_cloned_cache and destroy
//   is that the former one only free the resources that are
//   unique to the cacheAlgo, freeing these resources won't affect
//   other caches copied from original cacheAlgo
//   in Optimal, next_access should not be freed in destroy_cloned_cache,
//   because it is shared between different caches copied from the original one.
//   */
//
//  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
//  ARC_params->LRU1->core.destroy(ARC_params->LRU1);
//  ARC_params->LRU1g->core.destroy(ARC_params->LRU1g);
//  ARC_params->LRU2->core.destroy(ARC_params->LRU2);
//  ARC_params->LRU2g->core.destroy(ARC_params->LRU2g);
//  cache_destroy_cloned_cache(cache);
//}

cache_t *ARC_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("ARC", ccache_params);
  cache->cache_params = g_new0(struct ARC_params, 1);
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  ARC_init_params_t *init_params = (ARC_init_params_t *) cache_specific_init_params;

  cache->core.cache_specific_init_params = cache_specific_init_params;
//  cacheAlgo->core.add_only = ARC_add;

  ARC_params->ghost_list_factor = init_params->ghost_list_factor;

  // is this wrong? should size be size//2
  ARC_params->LRU1 = LRU_init(ccache_params, NULL);
  ARC_params->LRU2 = LRU_init(ccache_params, NULL);

  ccache_params.cache_size *= ARC_params->ghost_list_factor;
  ARC_params->LRU1g = LRU_init(ccache_params, NULL);
  ARC_params->LRU2g = LRU_init(ccache_params, NULL);

  return cache;
}

guint64 ARC_get_size(cache_t *cache) {
  ARC_params_t *ARC_params = (ARC_params_t *) (cache->cache_params);
  return (guint64) (ARC_params->size1 + ARC_params->size2);
}

#ifdef __cplusplus
}
#endif

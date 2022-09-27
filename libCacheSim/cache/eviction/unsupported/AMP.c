//
//  AMP.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "AMP.h"

#ifdef __cplusplus
extern "C" {
#endif

/** because we use direct_hash in hashtable, meaning we direct use pointer as
 *int, so be careful
 ** we require the block number to be within 32bit uint
 **/

#define GET_BLOCK(cp)                                                          \
  (((request_t *)(cp))->obj_id_type == OBJ_ID_NUM                                      \
       ? *(gint64 *)(((request_t *)(cp))->obj_id_ptr)                           \
       : atoll(((request_t *)(cp))->obj_id_ptr))

//#define TRACKED_BLOCK 13113277

void checkHashTable(gpointer key, gpointer value, gpointer user_data);

/* AMP functions */
struct AMP_page *AMP_lookup(cache_t *AMP, gint64 block) {
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);
  GList *list_node =
      (GList *)g_hash_table_lookup(AMP_params->hashtable, &block);
  if (list_node)
    return (struct AMP_page *)(list_node->data);
  else
    return NULL;
}

// static struct AMP_page* AMP_prev(cache_t* AMP, struct AMP_page* page){
//    return AMP_lookup(AMP, page->block_number - 1);
//}
//
// static struct AMP_page* AMP_prev_int(cache_t* AMP, gint64 block){
//    return AMP_lookup(AMP, block - 1);
//}

static struct AMP_page *AMP_last(cache_t *AMP, struct AMP_page *page) {
  return AMP_lookup(AMP, page->last_block_number);
}

static gboolean AMP_isLast(struct AMP_page *page) {
  return (page->block_number == page->last_block_number);
}

void createPages_no_eviction(cache_t *AMP, gint64 block_begin, gint length) {
  /* this function currently is used for prefetching */
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);
  if (length <= 0 || block_begin <= 1) {
    fprintf(stderr, "error AMP prefetch length %d, begin from %ld\n", length,
            (long)block_begin);
    abort();
  }
  //    printf("prefetch %d pages\n", length);

#ifdef TRACKED_BLOCK
  if (block_begin <= TRACKED_BLOCK && TRACKED_BLOCK < block_begin + length)
    printf("prefetch %ld, length %d\n", block_begin, length);
#endif

  gint64 i;
  gint64 lastblock = block_begin + length - 1;
  struct AMP_page *page_new;
  for (i = block_begin; i < block_begin + length; i += 1) {
    if (AMP_check_int(AMP, i))
      page_new = _AMP_update_int(AMP, i);
    else {
      AMP_params->num_of_prefetch++;
      page_new = _AMP_insert_int(AMP, i);
      g_hash_table_add(AMP_params->prefetched, &(page_new->block_number));
    }
    page_new->last_block_number = lastblock;
    page_new->accessed = 0;
    page_new->old = 0;
  }

  struct AMP_page *last_page = AMP_lookup(AMP, lastblock);
  struct AMP_page *prev_page = AMP_lookup(AMP, block_begin - 1);
  if (last_page ==
      NULL) { // || prev_page == NULL){       // prev page be evicted?
    ERROR("got NULL for page %p %p, block %ld %ld\n", prev_page, last_page,
          (long)(block_begin - 1), (long)lastblock);
  }

  // new 1704
  if (prev_page == NULL) {
    last_page->p = (gint16)length;
    last_page->g = (gint16)(last_page->p / 2);
    AMP_lookup(AMP, last_page->block_number)->tag = TRUE;
  }
  // end
  else {
    last_page->p = (gint16)MAX(prev_page->p, last_page->g + 1);
    last_page->g = prev_page->g;
    AMP_lookup(AMP, last_page->block_number - prev_page->g)->tag = TRUE;
  }
}

void createPages(cache_t *AMP, gint64 block_begin, gint length) {
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);
  createPages_no_eviction(AMP, block_begin, length);
  while ((long)g_hash_table_size(AMP_params->hashtable) > AMP->size)
    _AMP_evict(AMP, NULL);
}

struct AMP_page *lastInSequence(cache_t *AMP, struct AMP_page *page) {
  struct AMP_page *l1 = AMP_last(AMP, page);
  if (!l1)
    return 0;
  if (!AMP_lookup(AMP, page->last_block_number + 1))
    return l1;
  struct AMP_page *l2 = AMP_lookup(AMP, page->last_block_number + l1->p);
  if (l2)
    return l2;
  else
    return l1;
}

/* end of AMP functions */

void checkHashTable(gpointer key, gpointer value, gpointer user_data) {
  GList *node = (GList *)value;
  struct AMP_page *page = node->data;
  if (page->block_number != *(gint64 *)key || page->block_number < 0)
    printf("find error in page, page block %ld, key %ld\n",
           (long)page->block_number, *(long *)key);
  if (page->p < page->g) {
    ERROR("page %ld, p %d, g %d\n", (long)page->block_number, page->p, page->g);
    abort();
  }
}

struct AMP_page *_AMP_insert_int(cache_t *AMP, gint64 block) {
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);

  struct AMP_page *page = g_new0(struct AMP_page, 1);
  page->block_number = block;
  // accessed is set in add

  GList *node = g_list_alloc();
  node->data = page;

  g_queue_push_tail_link(AMP_params->list, node);
  g_hash_table_insert(AMP_params->hashtable, (gpointer) & (page->block_number),
                      (gpointer)node);
  return page;
}

void _AMP_insert(cache_t *AMP, request_t *cp) {
  gint64 block = GET_BLOCK(cp);
  _AMP_insert_int(AMP, block);
}

gboolean AMP_check_int(cache_t *cache, gint64 block) {
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  return g_hash_table_contains(AMP_params->hashtable, &block);
}

gboolean AMP_check(cache_t *cache, request_t *cp) {
  gint64 block = GET_BLOCK(cp);
  return AMP_check_int(cache, block);
}

struct AMP_page *_AMP_update_int(cache_t *cache, gint64 block) {
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  GList *node = g_hash_table_lookup(AMP_params->hashtable, &block);

  g_queue_unlink(AMP_params->list, node);
  g_queue_push_tail_link(AMP_params->list, node);
  return (struct AMP_page *)(node->data);
}

void _AMP_update(cache_t *cache, request_t *cp) {
  gint64 block = GET_BLOCK(cp);
  _AMP_update_int(cache, block);
}

void _AMP_evict(cache_t *AMP, request_t *cp) {
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);

  struct AMP_page *page = (struct AMP_page *)g_queue_pop_head(AMP_params->list);
  if (page->old || page->accessed) {
#ifdef TRACKED_BLOCK
    if (page->block_number == TRACKED_BLOCK)
      printf("ts %lu, final evict %d, old %d, accessed %d\n", cp->ts,
             TRACKED_BLOCK, page->old, page->accessed);
#endif
    gboolean result = g_hash_table_remove(
        AMP_params->hashtable, (gconstpointer) & (page->block_number));
    if (result == FALSE) {
      fprintf(stderr, "ERROR nothing removed, block %ld\n",
              (long)page->block_number);
      exit(1);
    }
    g_hash_table_remove(AMP_params->prefetched, &(page->block_number));
    page->block_number = -1;
    g_free(page);
  } else {
#ifdef TRACKED_BLOCK
    if (page->block_number == TRACKED_BLOCK)
      printf("ts %lu, try evict %d, old %d, accessed %d\n", cp->ts,
             TRACKED_BLOCK, page->old, page->accessed);
#endif

    page->old = TRUE;
    page->tag = FALSE;
    GList *node = g_list_alloc();
    node->data = page;
    g_queue_push_tail_link(AMP_params->list, node);
    g_hash_table_insert(AMP_params->hashtable,
                        (gpointer) & (page->block_number), (gpointer)node);

    struct AMP_page *last_sequence_page = lastInSequence(AMP, page);
    if (last_sequence_page) {
      last_sequence_page->p--;
      if (last_sequence_page->p < 1)
        last_sequence_page->p = 1;
      last_sequence_page->g =
          MIN(last_sequence_page->p - 1, last_sequence_page->g - 1);
      if (last_sequence_page->g < 0)
        last_sequence_page->g = 0;
    }
  }
}

void *_AMP_evict_with_return(cache_t *AMP, request_t *cp) {
  /** return a pointer points to the data that being evicted,
   *  it can be a pointer pointing to gint64 or a pointer pointing to char*
   *  it is the user's responsbility to g_free the pointer
   **/

  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);

  struct AMP_page *page = (struct AMP_page *)g_queue_pop_head(AMP_params->list);
  if (page->old || page->accessed) {
    gboolean result = g_hash_table_remove(
        AMP_params->hashtable, (gconstpointer) & (page->block_number));
    if (result == FALSE) {
      fprintf(stderr, "ERROR nothing removed, block %ld\n",
              (long)page->block_number);
      exit(1);
    }
    gint64 *return_data = g_new(gint64, 1);
    *(gint64 *)return_data = page->block_number;

    g_hash_table_remove(AMP_params->prefetched, &(page->block_number));
    page->block_number = -1;
    g_free(page);
    return return_data;
  } else {
    page->old = TRUE;
    page->tag = FALSE;
    GList *node = g_list_alloc();
    node->data = page;
    g_queue_push_tail_link(AMP_params->list, node);
    g_hash_table_insert(AMP_params->hashtable,
                        (gpointer) & (page->block_number), (gpointer)node);

    struct AMP_page *last_sequence_page = lastInSequence(AMP, page);
    if (last_sequence_page) {
      last_sequence_page->p--;
      if (last_sequence_page->p < 1)
        last_sequence_page->p = 1;
      last_sequence_page->g =
          MIN(last_sequence_page->p - 1, last_sequence_page->g - 1);
      if (last_sequence_page->g < 0)
        last_sequence_page->g = 0;
    }
    return _AMP_evict_with_return(AMP, cp);
  }
}

gboolean AMP_add_no_eviction(cache_t *AMP, request_t *cp) {
  struct AMP_params *AMP_params = (struct AMP_params *)(AMP->cache_params);
  gint64 block;
  if (cp->obj_id_type == OBJ_ID_NUM)
//    block = *(gint64 *)(cp->obj_id_ptr);
    block = GPOINTER_TO_SIZE(cp->obj_id_ptr);
  else {
    block = atoll(cp->obj_id_ptr);
  }

  struct AMP_page *page = AMP_lookup(AMP, block);

  if (AMP_check_int(AMP, block)) {
    // sanity check
    if (page == NULL)
      fprintf(stderr, "ERROR page is NULL\n");

    if (g_hash_table_contains(AMP_params->prefetched, &block)) {
      AMP_params->num_of_hit++;
      g_hash_table_remove(AMP_params->prefetched, &block);
    }

    if (page->accessed)
      _AMP_update_int(AMP, block);
    page->accessed = 1;

    gint length = 0;
    if (page->tag) {
      // hit in the trigger and prefetch
      struct AMP_page *page_last = AMP_last(AMP, page);
      length = AMP_params->read_size;
      if (page_last && page_last->p)
        length = page_last->p;
    }

    gboolean check_result = AMP_isLast(page) && !(page->old);
    if (check_result) {
      struct AMP_page *page_new = lastInSequence(AMP, page);
      if (page_new)
        if (page_new->p + AMP_params->read_size < AMP_params->p_threshold)
          page_new->p = page_new->p + AMP_params->read_size;
      ;
    }

    /* this is done here, because the page may be evicted when createPages */
    if (page->tag) {
      page->tag = FALSE;
      createPages_no_eviction(AMP, page->last_block_number + 1, length);
    }
    return TRUE;
  } else {
    if (page != NULL)
      fprintf(stderr, "ERROR page is not NULL\n");
    page = _AMP_insert_int(AMP, block);
    page->accessed = 1;

    struct AMP_page *page_prev = AMP_lookup(AMP, block - 1);
    int length = AMP_params->read_size;
    if (page_prev && page_prev->p)
      length = page_prev->p;
    // miss -> prefetch
    if (page_prev) {
      gboolean check = TRUE;
      int m;
      for (m = 2; m <= AMP_params->K; m++)
        check = check && AMP_lookup(AMP, block - m);
      if (check)
        createPages_no_eviction(AMP, block + 1, length);
    }

    return FALSE;
  }
}

gboolean AMP_add_only_no_eviction(cache_t *AMP, request_t *cp) {

  gint64 block;
  if (cp->obj_id_type == OBJ_ID_NUM)
//    block = *(gint64 *)(cp->obj_id_ptr);
    block = (gint64) GPOINTER_TO_SIZE(cp->obj_id_ptr);
  else {
    block = atoll(cp->obj_id_ptr);
  }

  struct AMP_page *page = AMP_lookup(AMP, block);

  if (AMP_check_int(AMP, block)) { // check is same
    // sanity check
    if (page == NULL)
      fprintf(stderr, "ERROR page is NULL\n");

    _AMP_update_int(AMP, block); // update is same
    page->accessed = 1;

    return TRUE;
  } else {
    if (page != NULL)
      fprintf(stderr, "ERROR page is not NULL\n");
    page = _AMP_insert_int(AMP, block); // insert is same
    page->accessed = 1;

    return FALSE;
  }
}

gboolean AMP_add_only(cache_t *cache, request_t *cp) {
  /* only add element, do not trigger other possible operation */

#ifdef TRACKED_BLOCK
  if ((gint64) GPOINTER_TO_SIZE(cp->obj_id_ptr) == TRACKED_BLOCK)
    printf("ts %lu, add_only %d\n", cp->ts, TRACKED_BLOCK);
#endif
  gboolean ret_val;
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  ret_val = AMP_add_only_no_eviction(cache, cp);
  while ((long)g_hash_table_size(AMP_params->hashtable) > cache->size)
    _AMP_evict(cache, cp); // not sure
  return ret_val;
}

gboolean AMP_add_no_eviction_withsize(cache_t *cache, request_t *cp) {
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  gint64 block;

//  if (cp->disk_sector_size != 0 && cache->block_size != 0) {
//    *(gint64 *)(cp->obj_id_ptr) =
//        (gint64)(*(gint64 *)(cp->obj_id_ptr) * cp->disk_sector_size /
//                 cache->block_size);
//    n = (int)ceil((double)cp->size / cache->block_size);
//    if (n < 1) // some traces have size zero for some requests
//      n = 1;
//  }

  block = (gint64) GPOINTER_TO_SIZE(cp->obj_id_ptr);
  struct AMP_page *page = AMP_lookup(cache, block);

  if (AMP_check_int(cache, block)) {
    // sanity check
    if (page == NULL)
      ERROR("check shows exist, but page is NULL\n");

    if (g_hash_table_contains(AMP_params->prefetched, &block)) {
      AMP_params->num_of_hit++;
      g_hash_table_remove(AMP_params->prefetched, &block);
    }

    if (page->accessed)
      _AMP_update_int(cache, block);
    page->accessed = 1;

    // new for withsize, keep reading the remaining pages
//    if (cp->disk_sector_size != 0 && cache->block_size != 0) {
//      gint64 old_block = (*(gint64 *)(cp->obj_id_ptr));
//      for (i = 0; i < n - 1; i++) {
//        (*(gint64 *)(cp->obj_id_ptr))++;
//        AMP_add_only(cache, cp);
//      }
//      (*(gint64 *)(cp->obj_id_ptr)) = old_block;
//    }
    // end

    gint length = 0;
    if (page->tag) {
      // hit in the trigger and prefetch
      struct AMP_page *page_last = AMP_last(cache, page);
      length = AMP_params->read_size;
      if (page_last && page_last->p)
        length = page_last->p;
    }

    gboolean check_result = AMP_isLast(page) && !(page->old);
    if (check_result) {
      struct AMP_page *page_new = lastInSequence(cache, page);
      if (page_new)
        if (page_new->p + AMP_params->read_size < AMP_params->p_threshold)
          page_new->p = page_new->p + AMP_params->read_size;
      ;
    }

    /* this is done here, because the page may be evicted when createPages */
    if (page->tag) {
      // this page can come from prefetch or read at miss
      page->tag = FALSE;
      createPages_no_eviction(cache, page->last_block_number + 1, length);
    }

    return TRUE;
  }
  // cache miss, load from disk
  else {
    if (page != NULL)
      ERROR("check non-exist, page is not NULL\n");
    page = _AMP_insert_int(cache, block);
    page->accessed = 1;
    struct AMP_page *page_prev = AMP_lookup(cache, block - 1);
    // NEED TO SET LAST PAGE
    AMP_lookup(cache, block)->last_block_number = block;

    // new for withsize, keep reading the remaining pages
    /*
    if (cp->disk_sector_size != 0 && cache->block_size != 0) {
      gint64 last_block = (*(gint64 *)(cp->obj_id_ptr)) + n - 1;
      // update new last_block_number
      AMP_lookup(cache, block)->last_block_number = last_block;

      for (i = 0; i < n - 1; i++) {
        (*(gint64 *)(cp->obj_id_ptr))++;

        // added 170505, for calculating precision
        //                if (g_hash_table_contains(AMP_params->prefetched,
        //                cp->obj_id_ptr)){
        //                    AMP_params->num_of_hit ++;
        //                    g_hash_table_remove(AMP_params->prefetched,
        //                    cp->obj_id_ptr);
        //                }

        AMP_add_only(cache, cp);
        AMP_lookup(cache, (*(gint64 *)(cp->obj_id_ptr)))->last_block_number =
            last_block;
      }
      *(gint64 *)(cp->obj_id_ptr) -= (n - 1);

#ifdef SANITY_CHECK
      if (*(gint64 *)(cp->obj_id_ptr) != block)
        ERROR("current block %ld, original %ld, n %d\n",
              *(gint64 *)(cp->obj_id_ptr), block, n);
      if (AMP_lookup(cache, block) == NULL)
        ERROR("requested block is not in cache after inserting, n %d, cache "
              "size %ld\n",
              n, eviction->size);
#endif

      // if n==1, then page_last is the same only page
      struct AMP_page *page_last = AMP_lookup(eviction, last_block);
      page_last->p = (page_prev ? page_prev->p : 0) + AMP_params->read_size;
      if (page_last->p > (int)(AMP_params->APT)) {
        page_last->g = (int)(AMP_params->APT / 2);
        struct AMP_page *tag_page = AMP_lookup(
            eviction, page_last->block_number - (int)(AMP_params->APT / 2));
        if (tag_page) {
          tag_page->tag = TRUE;
          tag_page->last_block_number = last_block;
        }
      }
      // this is possible, but not mentioned in the paper, anything wrong?
      if (page_last->p < page_last->g)
        page_last->g = page_last->p - 1 > 0 ? page_last->p - 1 : 0;
    } */
    // end

    // prepare for prefetching on miss
    int length = AMP_params->read_size;
    if (page_prev && page_prev->p)
      length = page_prev->p;

    // miss -> prefetch
    if (page_prev) {
      gboolean check = TRUE;
      int m; // m begins with 2, because page_prev already exists
      for (m = 2; m <= AMP_params->K; m++)
        check = check && AMP_lookup(cache, block - m);
      if (check) {
        // new 170505, for calculating precision
        //                if (cp->disk_sector_size != 0 &&
        //                eviction->block_size != 0){
        //                    block = (*(gint64*)(cp->obj_id_ptr)) + n -1;
        //                    length -= (long)(n - 1);
        //                }
        //                if (length > 0)
        //                    createPages_no_eviction(eviction, block + 1, length);
        // end
        createPages_no_eviction(cache, block + 1, length);
      }
    }

    return FALSE;
  }
}

gboolean AMP_add_withsize(cache_t *cache, request_t *cp) {
#ifdef TRACKED_BLOCK
  // debug
  if ((gint64) GPOINTER_TO_SIZE(cp->obj_id_ptr) == TRACKED_BLOCK)
    printf("ts %lu, add %d\n", cp->ts, TRACKED_BLOCK);
#endif
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);

  gboolean ret_val = AMP_add_no_eviction_withsize(cache, cp);
  while ((long)g_hash_table_size(AMP_params->hashtable) > cache->size)
    _AMP_evict(cache, cp);

  cache->ts += 1;
  return ret_val;
}

gboolean AMP_add(cache_t *cache, request_t *cp) {
#ifdef TRACKED_BLOCK
  // debug
  if ((gint64) GPOINTER_TO_SIZE(cp->obj_id_ptr) == TRACKED_BLOCK)
    printf("ts %lu, add %d\n", cp->ts, TRACKED_BLOCK);
#endif

  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  gboolean result = AMP_add_no_eviction(cache, cp);
  while ((long)g_hash_table_size(AMP_params->hashtable) > cache->size)
    _AMP_evict(cache, cp);
  cache->ts += 1;
  return result;
}

void AMP_destroy(cache_t *cache) {
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);

  g_queue_free_full(AMP_params->list, g_free);
  g_hash_table_destroy(AMP_params->hashtable);
  g_hash_table_destroy(AMP_params->prefetched);
  cache_destroy(cache);
}

void AMP_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the eviction, freeing these resources won't affect
   other caches copied from original eviction
   */
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);

  g_queue_free(AMP_params->list);
  g_hash_table_destroy(AMP_params->hashtable);
  g_hash_table_destroy(AMP_params->prefetched);
}

cache_t *AMP_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("AMP", size, obj_id_type);
  cache->cache_params = g_new0(struct AMP_params, 1);
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  struct AMP_init_params *init_params = (struct AMP_init_params *)params;

  cache->cache_init = AMP_init;
  cache->destroy = AMP_destroy;
  cache->destroy_unique = AMP_destroy_unique;
  cache->add = AMP_add;
  cache->check = AMP_check;
  cache->_insert = _AMP_insert;
  cache->_update = _AMP_update;
  cache->_evict = _AMP_evict;
  cache->evict_with_return = _AMP_evict_with_return;

  cache->get_used_size = AMP_get_size;
  cache->cache_init_params = params;
//  eviction->add_only = AMP_add_only;
//  eviction->add_withsize = AMP_add_withsize;

  AMP_params->K = init_params->K;
  AMP_params->APT = init_params->APT;
  AMP_params->read_size = init_params->read_size;
  AMP_params->p_threshold = init_params->p_threshold;

  AMP_params->hashtable =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  AMP_params->prefetched =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
  AMP_params->list = g_queue_new();

  return cache;
}

guint64 AMP_get_size(cache_t *cache) {
  struct AMP_params *AMP_params = (struct AMP_params *)(cache->cache_params);
  return (guint64)g_hash_table_size(AMP_params->hashtable);
}

#ifdef __cplusplus
}
#endif

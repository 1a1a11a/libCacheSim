//
// Created by Juncheng Yang on 3/21/20.
//


#include "cacheAlgUtils.h"


void queue_node_destroyer(gpointer data) {
  GList* node = (GList*) data;
  cache_obj_t *cache_obj = (cache_obj_t *) node->data;
  if (cache_obj->extra_data_ptr)
    g_free(cache_obj->extra_data_ptr);
  g_free(cache_obj);
//  g_free(node);
}


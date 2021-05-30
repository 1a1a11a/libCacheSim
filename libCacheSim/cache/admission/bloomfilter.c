//
// Created by Juncheng on 5/29/21.
//

#include "../include/libCacheSim/admissionAlgo.h"
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bloomfilter_admissioner {
  GHashTable *seen_times;
};


bool bloomfilter_admit(void *bloomfilter_admissioner, request_t *req) {
  struct bloomfilter_admissioner *ba = bloomfilter_admissioner;
  gpointer key = GINT_TO_POINTER(req->obj_id);
  gpointer n_times = g_hash_table_lookup(ba->seen_times, key);
  bool admit = (n_times == NULL);
  g_hash_table_replace(ba->seen_times, key, GINT_TO_POINTER(GPOINTER_TO_INT(n_times) + 1));

  return admit;
}

void *create_bloomfilter_admissioner() {
  struct bloomfilter_admissioner *ba = my_malloc(struct bloomfilter_admissioner);
  memset(ba, 0, sizeof(struct bloomfilter_admissioner));
  ba->seen_times = g_hash_table_new(g_int64_hash, g_int64_equal);

  return ba;
}

void free_bloomfilter_admissioner(void *s) {
  struct bloomfilter_admissioner *ba = s;
  g_hash_table_destroy(ba->seen_times);
  my_free(sizeof(struct bloomfilter_admissioner), s);
}

#ifdef __cplusplus
}
#endif

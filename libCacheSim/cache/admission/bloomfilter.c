//
// Created by Juncheng on 5/29/21.
//

#include <glib.h>
#include <stdbool.h>

#include "../../include/libCacheSim/admissionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bloomfilter_admission {
  GHashTable *seen_times;
} bf_admission_params_t;

bool bloomfilter_admit(admissioner_t *admissioner, const request_t *req) {
  bf_admission_params_t *bf = admissioner->params;
  gpointer key = GINT_TO_POINTER(req->obj_id);
  gpointer n_times =
      g_hash_table_lookup(bf->seen_times, GSIZE_TO_POINTER(req->obj_id));
  if (n_times == NULL) {
    g_hash_table_insert(bf->seen_times, key, GINT_TO_POINTER(1));
    return false;
  } else {
    g_hash_table_insert(bf->seen_times, key,
                        GINT_TO_POINTER(GPOINTER_TO_INT(n_times) + 1));
    return true;
  }
}

admissioner_t *clone_bloomfilter_admissioner(admissioner_t *admissioner) {
  return create_bloomfilter_admissioner(admissioner->init_params);
}

void free_bloomfilter_admissioner(admissioner_t *admissioner) {
  struct bloomfilter_admission *bf = admissioner->params;
  g_hash_table_destroy(bf->seen_times);
  free(bf);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

admissioner_t *create_bloomfilter_admissioner(const char *init_params) {
  if (init_params != NULL) {
    ERROR("bloomfilter admission does not take any parameters");
  }

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  admissioner->init_params = NULL;

  bf_admission_params_t *bf_params =
      (bf_admission_params_t *)malloc(sizeof(bf_admission_params_t));
  memset(bf_params, 0, sizeof(struct bloomfilter_admission));
  bf_params->seen_times = g_hash_table_new(g_direct_hash, g_direct_equal);

  admissioner->params = bf_params;
  admissioner->clone = clone_bloomfilter_admissioner;
  admissioner->free = free_bloomfilter_admissioner;
  admissioner->admit = bloomfilter_admit;

  strncpy(admissioner->admissioner_name, "BloomFilter", CACHE_NAME_LEN);
  return admissioner;
}

#ifdef __cplusplus
}
#endif

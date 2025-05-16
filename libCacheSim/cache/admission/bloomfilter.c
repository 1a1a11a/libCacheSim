//
// Created by Juncheng on 5/29/21.
//

#include <glib.h>
#include <stdbool.h>

#include "../../include/libCacheSim/hashmap.h"
#include "../../include/libCacheSim/hashmap_defs.in"
#include "../../include/libCacheSim/admissionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bloomfilter_admission {
  hashmap_t *seen_times;
} bf_admission_params_t;

bool bloomfilter_admit(admissioner_t *admissioner, const request_t *req) {
  bf_admission_params_t *bf = admissioner->params;
  void *key = (void *)(req->obj_id);
  void *n_times =
      hashmap_get(bf->seen_times, (const void *)key, sizeof(obj_id_t));
  if (n_times == NULL) {
    hashmap_put(bf->seen_times, (const void *)key, sizeof(obj_id_t),
                (void *)(1));
    return false;
  } else {
    // g_hash_table_insert(..., GINT_TO_POINTER(GPOINTER_TO_INT(n_times) + 1))
    hashmap_put(bf->seen_times, (const void *)key, sizeof(obj_id_t),
                ((void *) (long) (((int) (long) (n_times)) + 1)));
    return true;
  }
}

admissioner_t *clone_bloomfilter_admissioner(admissioner_t *admissioner) {
  return create_bloomfilter_admissioner(admissioner->init_params);
}

void free_bloomfilter_admissioner(admissioner_t *admissioner) {
  struct bloomfilter_admission *bf = admissioner->params;
  hashmap_destroy(bf->seen_times);
  free(bf->seen_times);
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
  memset(admissioner, 0, sizeof(admissioner_t));
  admissioner->init_params = NULL;

  bf_admission_params_t *bf_params =
      (bf_admission_params_t *)malloc(sizeof(bf_admission_params_t));
  memset(bf_params, 0, sizeof(struct bloomfilter_admission));
  bf_params->seen_times = (hashmap_t *)malloc(sizeof(hashmap_t));
  hashmap_create_options_t options = {
      .initial_capacity = 16,
      .comparer = obj_id_comparer,
      .hasher = obj_id_hasher};
  hashmap_create_ex(options, bf_params->seen_times);

  admissioner->params = bf_params;
  admissioner->clone = clone_bloomfilter_admissioner;
  admissioner->free = free_bloomfilter_admissioner;
  admissioner->admit = bloomfilter_admit;

  strncpy(admissioner->admissioner_name, "BloomFilter", CACHE_NAME_LEN - 1);
  admissioner->admissioner_name[CACHE_NAME_LEN - 1] = '\0';
  return admissioner;
}

#ifdef __cplusplus
}
#endif

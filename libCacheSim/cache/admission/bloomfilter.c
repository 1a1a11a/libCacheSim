/**
 * @file bloomfilter.c
 * @brief Implementation of a Bloom filter-like admission policy.
 *
 * This admission policy uses a hash table to track the number of times an
 * object has been seen. It only admits an object into the cache upon its
 * second request. This helps to filter out one-hit wonders that would
 * otherwise pollute the cache.
 *
 * Note: Despite the name, this implementation uses a hash table for exact
 * counting, not a probabilistic Bloom filter data structure.
 */

#include <glib.h>
#include <stdbool.h>

#include "libCacheSim/admissionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters for the bloom filter admissioner.
 */
typedef struct bloomfilter_admission {
  GHashTable *seen_times; /**< A GLib hash table to store object IDs and their access counts. */
} bf_admission_params_t;

/**
 * @brief Decides whether to admit a request based on access history.
 *
 * This function checks a hash table for the request's object ID.
 * - If the object has not been seen before, it is added to the table with a
 *   count of 1, and the function returns `false` (do not admit).
 * - If the object has been seen before, its count is incremented, and the
 *   function returns `true` (admit).
 *
 * @param admissioner The admissioner instance.
 * @param req The request to consider for admission.
 * @return True to admit the object, false otherwise.
 */
bool bloomfilter_admit(admissioner_t *admissioner, const request_t *req) {
  bf_admission_params_t *bf = admissioner->params;
  gpointer key = GINT_TO_POINTER(req->obj_id);
  gpointer n_times =
      g_hash_table_lookup(bf->seen_times, GSIZE_TO_POINTER(req->obj_id));
  if (n_times == NULL) {
    // First time seeing this object, don't admit yet.
    g_hash_table_insert(bf->seen_times, key, GINT_TO_POINTER(1));
    return false;
  } else {
    // Second or later time, admit.
    g_hash_table_insert(bf->seen_times, key,
                        GINT_TO_POINTER(GPOINTER_TO_INT(n_times) + 1));
    return true;
  }
}

/**
 * @brief Clones a bloom filter admissioner.
 * @param admissioner The admissioner to clone.
 * @return A new admissioner instance with the same initial parameters.
 */
admissioner_t *clone_bloomfilter_admissioner(admissioner_t *admissioner) {
  return create_bloomfilter_admissioner(admissioner->init_params);
}

/**
 * @brief Frees the resources used by a bloom filter admissioner.
 * @param admissioner The admissioner to free.
 */
void free_bloomfilter_admissioner(admissioner_t *admissioner) {
  struct bloomfilter_admission *bf = admissioner->params;
  g_hash_table_destroy(bf->seen_times);
  free(bf);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

/**
 * @brief Creates and initializes a new bloom filter admissioner.
 * @param init_params Initialization parameters (not used by this admissioner).
 * @return A pointer to the newly created admissioner.
 */
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
  bf_params->seen_times = g_hash_table_new(g_direct_hash, g_direct_equal);

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

#ifndef CR_LFU_H
#define CR_LFU_H

#include <glib.h>

#include "../cache.h"
#include "LFU.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CR_LFU_params {
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;
  cache_t *other_cache;
} CR_LFU_params_t;

cache_t *CR_LFU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

void CR_LFU_free(cache_t *cache);

cache_ck_res_e CR_LFU_check(cache_t *cache, const request_t *req,
                            const bool update);

cache_ck_res_e CR_LFU_get(cache_t *cache, const request_t *req);

void CR_LFU_remove(cache_t *cache, const obj_id_t obj_id);

cache_obj_t *CR_LFU_insert(cache_t *CR_LFU, const request_t *req);

cache_obj_t *CR_LFU_to_evict(cache_t *cache);

void CR_LFU_evict(cache_t *CR_LFU, const request_t *req,
                  cache_obj_t *cache_obj);

#ifdef __cplusplus
}
#endif

#endif /* CR_LFU_H */

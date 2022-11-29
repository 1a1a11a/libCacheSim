//
//  BeladySize.c
//  libCacheSim
//
//  sample object and compare reuse_distance * size, then evict the greatest one
//
//
/* todo: change to BeladySize */

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/BeladySize.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define EXACT_Belady 1

typedef struct {
  int n_sample;
} BeladySize_params_t; /* BeladySize parameters */

static const char *BeladySize_current_params(BeladySize_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-sample=%d\n", params->n_sample);
  return params_str;
}

static void BeladySize_parse_params(cache_t *cache,
                                    const char *cache_specific_params) {
  BeladySize_params_t *params = (BeladySize_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by '=' */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "n-sample") == 0) {
      params->n_sample = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", BeladySize_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support %s\n", cache->cache_name,
            key, BeladySize_current_params(params));
      exit(1);
    }
  }

  free(old_params_str);
}

cache_t *BeladySize_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("BeladySize", ccache_params);

  cache->cache_init = BeladySize_init;
  cache->cache_free = BeladySize_free;
  cache->get = BeladySize_get;
  cache->check = BeladySize_check;
  cache->insert = BeladySize_insert;
  cache->evict = BeladySize_evict;
  cache->remove = BeladySize_remove;
  cache->to_evict = BeladySize_to_evict;

  BeladySize_params_t *params =
      (BeladySize_params_t *)malloc(sizeof(BeladySize_params_t));
  params->n_sample = 128;
  cache->eviction_params = params;

  if (cache_specific_params != NULL) {
    BeladySize_parse_params(cache, cache_specific_params);
  }

  return cache;
}

void BeladySize_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e BeladySize_check(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  cache_obj_t *obj;
  cache_ck_res_e ck = cache_check_base(cache, req, update_cache, &obj);

  if (!update_cache) return ck;

  if (update_cache && ck == cache_ck_hit) {
    if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
      BeladySize_remove(cache, obj->obj_id);
    } else {
      obj->Belady.next_access_vtime = req->next_access_vtime;
    }
  }

  return ck;
}

cache_ck_res_e BeladySize_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *BeladySize_insert(cache_t *cache, const request_t *req) {
  if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
    return NULL;
  }

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Belady.next_access_vtime = req->next_access_vtime;

  return obj;
}

#ifdef EXACT_Belady
struct hash_iter_user_data {
  uint64_t curr_vtime;
  cache_obj_t *to_evict_obj;
  uint64_t max_score;
};

static inline void hashtable_iter_Belady_size(cache_obj_t *cache_obj,
                                              void *userdata) {
  struct hash_iter_user_data *iter_userdata =
      (struct hash_iter_user_data *)userdata;
  if (iter_userdata->max_score == UINT64_MAX) return;

  uint64_t obj_score;
  if (cache_obj->Belady.next_access_vtime == -1)
    obj_score = UINT64_MAX;
  else
    obj_score = cache_obj->obj_size * (cache_obj->Belady.next_access_vtime -
                                       iter_userdata->curr_vtime);

  if (obj_score > iter_userdata->max_score) {
    iter_userdata->to_evict_obj = cache_obj;
    iter_userdata->max_score = obj_score;
  }
}

cache_obj_t *BeladySize_to_evict(cache_t *cache) {
  struct hash_iter_user_data iter_userdata;
  iter_userdata.curr_vtime = cache->n_req;
  iter_userdata.max_score = 0;
  iter_userdata.to_evict_obj = NULL;

  hashtable_foreach(cache->hashtable, hashtable_iter_Belady_size,
                    &iter_userdata);

  return iter_userdata.to_evict_obj;
}

#else
cache_obj_t *BeladySize_to_evict(cache_t *cache) {
  BeladySize_params_t *params = (BeladySize_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = NULL, *sampled_obj;
  int64_t obj_to_evict_score = -1, sampled_obj_score;
  for (int i = 0; i < params->n_sample; i++) {
    sampled_obj = hashtable_rand_obj(cache->hashtable);
    sampled_obj_score =
        (int64_t)sampled_obj->obj_size *
        (int64_t)(sampled_obj->Belady.next_access_vtime - cache->n_req);
    if (obj_to_evict_score < sampled_obj_score) {
      obj_to_evict = sampled_obj;
      obj_to_evict_score = sampled_obj_score;
    }
  }
  if (obj_to_evict == NULL) {
    WARN(
        "BeladySize_to_evict: obj_to_evict is NULL, "
        "maybe cache size is too small or hash power too large\n");
    return BeladySize_to_evict(cache);
  }

  return obj_to_evict;
}
#endif

void BeladySize_evict(cache_t *cache, const request_t *req,
                      cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict = BeladySize_to_evict(cache);
  if (cache_obj != NULL) {
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  cache_remove_obj_base(cache, obj_to_evict);
}

void BeladySize_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("obj to remove is not in the cache\n");
    return;
  }
  cache_remove_obj_base(cache, obj);
}

#ifdef __cplusplus
}
#endif

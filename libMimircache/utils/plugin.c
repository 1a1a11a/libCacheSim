//
// Created by Juncheng Yang on 11/17/19.
//


#include "../../CMimircache/include/mimircache/plugin.h"

cache_t *create_cache_external(const char *cache_alg_name, uint64_t size, obj_id_t obj_id_type, void *params) {
  void *handle;
  char *error;
  cache_t *(*cache_init)(guint64, char, guint64, void *);

  char shared_lib_path[256];
  char cache_init_func_name[256];
  sprintf(shared_lib_path, "./lib%s.so", cache_alg_name);
  sprintf(cache_init_func_name, "%s_init", cache_alg_name);

  handle = dlopen(shared_lib_path, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  dlerror();    /* Clear any existing error */

  *(void **) (&cache_init) = dlsym(handle, cache_init_func_name);

  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  } else {
    INFO("external cache %s loaded\n", cache_alg_name);
  }
  cache_t *cache = cache_init(size, obj_id_type, 0, params);

  // disable dlclose for now, we need a global pool of handles that we can track and close
//  dlclose(handle);
  return cache;
}


cache_t *create_cache_internal(const char *cache_alg_name, uint64_t size, obj_id_t obj_id_type, void *params) {
  cache_t *(*cache_init)(guint64, char, guint64, void *);

  char cache_init_func_name[256];
  sprintf(cache_init_func_name, "%s_init", cache_alg_name);
  *(void **) (&cache_init) = dlsym(NULL, cache_init_func_name);

  if ((dlerror()) != NULL) {
    return NULL;
  } else {
    DEBUG("internal cache %s loaded\n", cache_alg_name);
    cache_t *cache = cache_init(size, obj_id_type, 0, params);
    return cache;
  }
}


cache_t *create_cache(const char *cache_alg_name, uint64_t size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = create_cache_internal(cache_alg_name, size, obj_id_type, params);
  if (cache == NULL) {
    cache = create_cache_external(cache_alg_name, size, obj_id_type, params);
  }
  return cache;
}

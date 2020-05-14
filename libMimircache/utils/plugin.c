//
// Created by Juncheng Yang on 11/17/19.
//


#include "../include/mimircache/plugin.h"

#ifdef __cplusplus
extern "C"
{
#endif


cache_t *
create_cache_external(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params) {
  void *handle;
  char *error;
  cache_t *(*cache_init)(common_cache_params_t, void *);

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
  cache_t *cache = cache_init(cc_params, cache_specific_params);

  // disable dlclose for now, we need a global pool of handles that we can track and close
//  dlclose(handle);
  return cache;
}


cache_t *
create_cache_internal(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params) {
  cache_t *(*cache_init)(common_cache_params_t, void *);

  char cache_init_func_name[256];
  void *handle = dlopen(NULL, RTLD_GLOBAL);

  sprintf(cache_init_func_name, "%s_init", cache_alg_name);
  *(void **) (&cache_init) = dlsym(handle, cache_init_func_name);

  if ((dlerror()) != NULL) {
    WARNING("cannot load internal cache %s because cannot find %s\n", cache_alg_name, cache_init_func_name);
    return NULL;
  } else {
    DEBUG3("internal cache %s loaded\n", cache_alg_name);
    cache_t *cache = cache_init(cc_params, cache_specific_params);
    return cache;
  }
}


cache_t *create_cache(const char *const cache_alg_name, common_cache_params_t cc_params, void *cache_specific_params) {
  cache_t *cache = create_cache_internal(cache_alg_name, cc_params, cache_specific_params);
  if (cache == NULL) {
    cache = create_cache_external(cache_alg_name, cc_params, cache_specific_params);
  }
  if (cache == NULL) {
    ERROR("failed to create cache %s\n", cache_alg_name);
    abort();
  }
  return cache;
}

#ifdef __cplusplus
}
#endif
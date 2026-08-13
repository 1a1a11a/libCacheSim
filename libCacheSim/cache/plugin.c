//
// Created by Juncheng Yang on 11/17/19.
//

#include "libCacheSim/plugin.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *create_cache_external(const char *const cache_alg_name,
                               common_cache_params_t cc_params,
                               void *cache_specific_params) {
  void *handle;
  char *error;
  cache_t *(*cache_init)(common_cache_params_t, void *);

  char shared_lib_path[256];
  char cache_init_func_name[256];
  snprintf(shared_lib_path, sizeof(shared_lib_path), "./lib%s.so",
           cache_alg_name);
  snprintf(cache_init_func_name, sizeof(cache_init_func_name), "%s_init",
           cache_alg_name);

  /* Failure returns NULL, as the header documents, so the caller can report
   * which algorithm it could not find. Exiting here instead made that
   * reporting unreachable and left the user with a bare dlerror string. */
  handle = dlopen(shared_lib_path, RTLD_LAZY);
  if (!handle) {
    WARN("cannot load %s: %s\n", shared_lib_path, dlerror());
    return NULL;
  }
  dlerror(); /* Clear any existing error */

  // ISO C compliant way to convert void* to function pointer
  union {
    void *obj_ptr;
    cache_t *(*func_ptr)(common_cache_params_t, void *);
  } dlsym_ptr;

  dlsym_ptr.obj_ptr = dlsym(handle, cache_init_func_name);
  cache_init = dlsym_ptr.func_ptr;

  if ((error = dlerror()) != NULL) {
    WARN("cannot find %s in %s: %s\n", cache_init_func_name, shared_lib_path,
         error);
    return NULL;
  } else {
    INFO("external cache %s loaded\n", cache_alg_name);
  }
  cache_t *cache = cache_init(cc_params, cache_specific_params);

  // disable dlclose for now, we need a global pool of handles that we can track
  // and close
  //  dlclose(handle);
  return cache;
}

cache_t *create_cache_internal(const char *const cache_alg_name,
                               common_cache_params_t cc_params,
                               void *cache_specific_params) {
  /* Built-in algorithms are looked up in the registry rather than through
   * dlsym(). Their constructors live in an archive member that nothing else
   * references, so in a statically linked build the linker never pulls them in
   * and dlsym() cannot find them however the executable is linked. */
  cache_t *cache = create_cache_by_name(cache_alg_name, cc_params,
                                        (const char *)cache_specific_params);
  if (cache != NULL) {
    return cache;
  }

  /* Fall back to dlsym for an algorithm that is not built in, e.g. one loaded
   * into the process from elsewhere. */
  char cache_init_func_name[256];
  void *handle = dlopen(NULL, RTLD_GLOBAL);
  /* should not check err here, otherwise ubuntu will report err even though
   * everything is OK */

  snprintf(cache_init_func_name, sizeof(cache_init_func_name), "%s_init",
           cache_alg_name);

  // ISO C compliant way to convert void* to function pointer
  union {
    void *obj_ptr;
    cache_t *(*func_ptr)(common_cache_params_t, void *);
  } dlsym_ptr;

  dlsym_ptr.obj_ptr = dlsym(handle, cache_init_func_name);
  cache_t *(*cache_init)(common_cache_params_t, void *) = dlsym_ptr.func_ptr;

  if (cache_init == NULL) {
    /* Not an error yet: the caller falls back to loading a shared library. */
    (void)dlerror();
    return NULL;
  }

  INFO("internal cache %s loaded\n", cache_alg_name);
  return cache_init(cc_params, cache_specific_params);
}

cache_t *create_cache_using_plugin(const char *const cache_alg_name,
                                   common_cache_params_t cc_params,
                                   void *specific_params) {
  cache_t *cache =
      create_cache_internal(cache_alg_name, cc_params, specific_params);
  if (cache == NULL) {
    cache = create_cache_external(cache_alg_name, cc_params, specific_params);
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

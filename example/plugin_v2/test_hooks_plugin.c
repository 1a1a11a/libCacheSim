#include <assert.h>
#include <libCacheSim.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"

static const char *plugin_lib_name = "libplugin_lru_hooks.so";

int main(int argc, char *argv[]) {
  common_cache_params_t cache_params = {
      .cache_size = 1000,  // Small cache for easy testing
      .default_ttl = 0,
      .hashpower = 16,
      .consider_obj_metadata = false,
  };

  // create a plugin LRU cache
  char *curr_bin_path = strdup(argv[0]);
  char *curr_dir = dirname(curr_bin_path);
  char *plugin_path = malloc(strlen(curr_dir) + strlen(plugin_lib_name) + 20);
  sprintf(plugin_path, "plugin_path=%s/%s", curr_dir, plugin_lib_name);
  printf("plugin_path: %s\n", plugin_path);
  cache_t *plugin_cache = pluginCache_init(cache_params, plugin_path);
  assert(plugin_cache != NULL);

  request_t *req = new_request();
  assert(req != NULL);

  // create a LRU cache
  cache_t *lru_cache = LRU_init(cache_params, NULL);
  assert(lru_cache != NULL);

  // generate a 1000 random requests and compare the result of the plugin LRU
  // cache and the default LRU cache
  for (int i = 0; i < 1000; i++) {
    req->obj_id = rand() % 1000;
    req->obj_size = rand() % 10;
    req->clock_time = i;

    bool hit1 = plugin_cache->get(plugin_cache, req);
    bool hit2 = lru_cache->get(lru_cache, req);
    assert(hit1 == hit2);
  }

  // free the request
  free_request(req);
  plugin_cache->cache_free(plugin_cache);
  lru_cache->cache_free(lru_cache);
  printf("Plugin LRU cache and LRU cache are the same\n\n");
  return 0;
}

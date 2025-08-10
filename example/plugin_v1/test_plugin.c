#include <assert.h>
#include <dlfcn.h>
#include <libCacheSim.h>
#include <libCacheSim/plugin.h>
#include <libgen.h>

void test_plugin_lru(int argc, char *argv[]) {
  common_cache_params_t ccache_params = {.cache_size = 1000,
                                         .default_ttl = 0,
                                         .hashpower = 20,
                                         .consider_obj_metadata = false};

  // note that the plugin system assumes the plugin is in the same directory as
  // the test binary with a name of lib<plugin_name>.so
  cache_t *plugin_cache =
      create_cache_external("plugin_lru", ccache_params, NULL);
  cache_t *builtin_cache = create_cache_internal("LRU", ccache_params, NULL);

  assert(plugin_cache != NULL);
  assert(builtin_cache != NULL);

  // Test empty cache state
  assert(plugin_cache->get_n_obj(plugin_cache) == 0);
  assert(builtin_cache->get_n_obj(builtin_cache) == 0);

  for (int i = 0; i < 1000; i++) {
    request_t req = {.obj_id = i, .obj_size = 100};
    bool plugin_hit = plugin_cache->get(plugin_cache, &req);
    bool builtin_hit = builtin_cache->get(builtin_cache, &req);
    assert(plugin_hit == builtin_hit);
  }

  plugin_cache->cache_free(plugin_cache);
  builtin_cache->cache_free(builtin_cache);

  INFO("test_plugin_lru passed\n");
}

int main(int argc, char *argv[]) {
  test_plugin_lru(argc, argv);
  return 0;
}

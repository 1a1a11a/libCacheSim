# Plugin LRU Hooks Example

This example demonstrates how to create a plugin for libCacheSim using the hook-based system implemented in `plugin_cache.c`.

## Files

- `plugin_lru_hooks.cpp` - The main LRU cache plugin implementation with hooks
- `plugin_func.cpp` - Additional plugin functions
- `test_hooks_plugin.c` - Test program for the plugin
- `CMakeLists.txt` - Build configuration for creating a shared library

## Building

To compile the plugin into a shared library:

```bash
mkdir build
cd build
cmake ..
make
```

This will create:
- `libplugin_lru_hooks.so` - The shared library containing the plugin
- `test_hooks_plugin` - A test executable (if test file exists)

## Plugin Interface

The plugin implements the following hook functions expected by libCacheSim's plugin system:

- `cache_init_hook()` - Initialize the cache data structure
- `cache_hit_hook()` - Handle cache hits (move to head of LRU list)
- `cache_miss_hook()` - Handle cache misses (insert new object)
- `cache_eviction_hook()` - Evict least recently used object
- `cache_remove_hook()` - Remove specific object from cache

## Usage


```
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru,pluginCache 0.01,0.1 -e "plugin=/proj/cache-PG0/jason/libCacheSim/example/pluginv2/_build/libplugin_lru_hooks.so.1.0.0"
```

## Dependencies

- libCacheSim headers
- GLib (for basic data types)
- C++17 compiler
- CMake 3.12 or higher 
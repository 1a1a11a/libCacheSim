# Plugin V2 Example - LRU Cache with Hooks

This example demonstrates how to create a plugin for libCacheSim using the v2 hook-based plugin system. The plugin implements a standalone LRU (Least Recently Used) cache algorithm that integrates with libCacheSim's plugin_cache.c framework.

## Files

- `plugin_lru.cpp` - The main LRU cache plugin implementation using hooks
- `test_hooks_plugin.c` - Comprehensive test program for the plugin
- `CMakeLists.txt` - Build configuration for creating the shared library

## Building

To compile the plugin into a shared library:

```bash
# Prerequisites: Install Ninja build system if not already available
# Ubuntu/Debian: sudo apt install ninja-build
# macOS: brew install ninja

mkdir build
cd build
cmake -G Ninja ..
ninja
```

This will create:
- `libplugin_lru_hooks.so` - Shared library
- `test_hooks_plugin` - Test executable for validation

## Plugin Architecture

The plugin implements a standalone LRU cache using C++ with a C interface for compatibility with libCacheSim. It uses:

- **StandaloneLRU Class**: C++ implementation with doubly-linked list and hash map
- **Hook Functions**: C interface functions that plugin_cache.c calls

### Hook Functions

The plugin implements these required hook functions:

- `cache_init_hook()` - Initialize the LRU cache data structure
- `cache_hit_hook()` - Handle cache hits (move object to head of LRU list)
- `cache_miss_hook()` - Handle cache misses (insert new object)
- `cache_eviction_hook()` - Evict least recently used object and return its ID
- `cache_remove_hook()` - Remove specific object from cache
- `cache_free_hook()` - Clean up and free the LRU cache data structure

## Usage

### With cachesim Binary

```bash
# Run cachesim with the plugin
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru,pluginCache 0.01,0.1 \
  -e "plugin_path=/path/to/libCacheSim/example/plugin_v2/build/libplugin_lru_hooks.so"
```

### Testing the Plugin

Run the included test to verify plugin functionality:

```bash
cd build
./test_hooks_plugin
```

The test compares the plugin LRU implementation against libCacheSim's built-in LRU to ensure identical behavior.

## Dependencies

- **libCacheSim**: Headers and libraries from the main project
- **CMake 3.12+**: Build system

## Plugin Parameter Format

When using the plugin with cachesim or other libCacheSim tools, use the format:
```
plugin_path=/full/path/to/libplugin_lru_hooks.so
```

## Implementation Notes

- The plugin uses C++ internally but exports C functions for compatibility
- Memory management is handled automatically by the StandaloneLRU destructor
- The implementation maintains the same LRU semantics as libCacheSim's built-in LRU
- Thread safety is not implemented - suitable for single-threaded use cases

## Troubleshooting

- Ensure the plugin path uses the `plugin_path=` prefix format
- Verify all dependencies are available (check with `ldd libplugin_lru_hooks.so`)
- Use absolute paths when specifying the plugin location

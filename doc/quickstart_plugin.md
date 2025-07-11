# libCacheSim Plugin System – Quick-Start Guide

> **Audience**: Developers who want to add custom cache–replacement policies to *libCacheSim* without modifying the core library.
>
> **Goal**: Build a shared-library plugin that implements a few well-defined hook functions, then load it at runtime via `pluginCache`.

---

## 1 . How the Plugin System Works

`plugin_cache.c` ships with *libCacheSim* and delegates **all policy-specific logic** to a user-supplied shared library (``.so`` / ``.dylib``).  At run-time the library is

1. loaded with `dlopen()`;
2. each required *hook* is resolved with `dlsym()`; and
3. the hooks are invoked on cache hits, misses, evictions, and removals.

Because the plugin is completely decoupled from core code you can:
* experiment with new algorithms quickly,
* write the plugin in **C or C++**, and
* distribute it independently from *libCacheSim*.

### 1.1 Required Hook Functions

Your library **must** export the following C-symbols:

| Hook | Prototype | Called When |
|------|-----------|-------------|
| `cache_init_hook` | `void *cache_init_hook(const common_cache_params_t ccache_params);` | Once at cache creation. Return an opaque pointer to plugin state. |
| `cache_hit_hook` | `void cache_hit_hook(void *data, const request_t *req);` | A requested object is found in the cache. |
| `cache_miss_hook` | `void cache_miss_hook(void *data, const request_t *req);` | A requested object is **not** in the cache *after* insertion. |
| `cache_eviction_hook` | `obj_id_t cache_eviction_hook(void *data, const request_t *req);` | Cache is full – must return the object-ID to evict. |
| `cache_remove_hook` | `void cache_remove_hook(void *data, const obj_id_t obj_id);` | An object is explicitly removed (not necessarily due to eviction). |

The opaque pointer returned by `cache_init_hook` is passed back to every other hook via the `data` parameter, letting your plugin maintain arbitrary state (linked lists, hash maps, statistics, …). For memory safety, your library can export `cache_free_hook` (`void cache_free_hook(void *data);`) to free the resources used by your cache struct according to your demands.

---

## 2 . Minimal Plugin Skeleton (C++)

Below is an **abridged** version of the LRU example in `example/plugin_v2/plugin_lru.cpp`.  You can copy this as a starting point for your own policy:

```cpp
#include <libCacheSim.h>   // public headers installed by libCacheSim
#include <unordered_map>

class MyPolicy {
  /* your data structures */
public:
  MyPolicy() {/*init*/}
  void on_hit(obj_id_t id) {/*...*/}
  void on_miss(obj_id_t id, uint64_t size) {/*...*/}
  obj_id_t evict() {/* decide victim */}
  void on_remove(obj_id_t id) {/*...*/}
};

extern "C" {
void *cache_init_hook(const common_cache_params_t /*params*/) {
  return new MyPolicy();
}

void cache_hit_hook(void *data, const request_t *req) {
  static_cast<MyPolicy *>(data)->on_hit(req->obj_id);
}

void cache_miss_hook(void *data, const request_t *req) {
  static_cast<MyPolicy *>(data)->on_miss(req->obj_id, req->obj_size);
}

obj_id_t cache_eviction_hook(void *data, const request_t * /*req*/) {
  return static_cast<MyPolicy *>(data)->evict();
}

void cache_remove_hook(void *data, const obj_id_t obj_id) {
  static_cast<MyPolicy *>(data)->on_remove(obj_id);
}
} // extern "C"
```

*Notes*
1. The plugin can allocate dynamic memory; it will live until the cache is destroyed.
2. Thread safety is up to you – core *libCacheSim* is single-threaded today.

---

## 3 . Building the Plugin

### 3.1 Dependencies

* **CMake ≥ 3.12** (recommended)
* A C/C++ compiler (``gcc``, ``clang``)

### 3.2 Sample `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.12)
project(my_cache_plugin CXX C)

# Tell CMake to create a shared library
add_library(plugin_my_policy SHARED plugin_my_policy.cpp)

# Location of libCacheSim headers – adjust if you installed elsewhere
target_include_directories(plugin_my_policy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Position-independent code is implicit for shared libs but keep for clarity
set_property(TARGET plugin_my_policy PROPERTY POSITION_INDEPENDENT_CODE ON)

# Optional: strip symbols & set output name
set_target_properties(plugin_my_policy PROPERTIES
  OUTPUT_NAME "plugin_my_policy_hooks")
```

### 3.3 Build Commands

```bash
mkdir build && cd build
cmake -G Ninja ..   # or "cmake .. && make"
ninja               # produces libplugin_my_policy_hooks.so
```

> On macOS the file extension will be `.dylib` instead of `.so`.

---

## 4 . Using the Plugin with `cachesim`

1. **Compile** the plugin (`libplugin_my_policy_hooks.so`).
2. **Run** `cachesim` with `pluginCache` **and** supply `plugin_path=`:

```bash
./bin/cachesim data/cloudPhysicsIO.vscsi vscsi pluginCache 0.01 \
  -e "plugin_path=/absolute/path/libplugin_my_policy_hooks.so,cache_name=myPolicy"
```

* Keys after `-e` are comma-separated.  Supported keys today:
  * `plugin_path` (required) – absolute or relative path to the `.so` / `.dylib`.
  * `cache_name`   (optional) – override the cache’s display name.
  * `print`        – debug helper: print current parameters and exit.

If you omit `cache_name`, the runtime will default to `pluginCache-<fileName>` for easier identification in logs.

---

## 5 . A full example

A comprehensive example lives in `example/plugin_v2`.  After building the example plugin:

---

## 6 . Troubleshooting Checklist

* **Plugin not found?** Verify the path passed via `plugin_path=` is correct, you may want to use absolute path.
* **Missing symbols?** Make sure the function names exactly match the prototypes above and are declared `extern "C"` when compiling as C++.
* **Link-time errors?** Pass the same architecture flags (`-m64`, etc.) that *libCacheSim* was built with.
* **Runtime crash inside plugin?** Use `gdb -ex r --args cachesim …` and place breakpoints in your hook functions.

---


Happy caching!

# libCacheSim Python Binding

Python bindings for libCacheSim, a high-performance cache simulator.

## Installation

```bash
pip install .
```

## Development

```bash
pip install -e .
```

Test

```
python -m pytest .
```

## Usage

### Basic Cache Usage

```python
import libcachesim as cachesim

# Create a cache with FIFO eviction policy
cache = cachesim.FIFO(cache_size=1024*1024)

# Create a request
req = cachesim.Request()
req.obj_id = 1
req.obj_size = 100

# Check if object is in cache
hit = cache.get(req)
print(f"Cache hit: {hit}")
```

### Custom Cache Policies

The Python binding supports custom cache replacement algorithms using Python function hooks - no C/C++ compilation required:

#### Python Hook Cache

Define custom cache policies using pure Python functions:

```python
import libcachesim as cachesim
from collections import OrderedDict

# Create a Python hook-based cache
cache = cachesim.PythonHookCachePolicy(cache_size=1024*1024, cache_name="MyLRU")

# Define LRU policy hooks
def init_hook(cache_size):
    return OrderedDict()  # Track access order

def hit_hook(lru_dict, obj_id, obj_size):
    lru_dict.move_to_end(obj_id)  # Move to end (most recent)

def miss_hook(lru_dict, obj_id, obj_size):
    lru_dict[obj_id] = True  # Add to end

def eviction_hook(lru_dict, obj_id, obj_size):
    return next(iter(lru_dict))  # Return least recent

def remove_hook(lru_dict, obj_id):
    lru_dict.pop(obj_id, None)

# Set the hooks
cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

# Use it like any other cache
req = cachesim.Request()
req.obj_id = 1
req.obj_size = 100
hit = cache.get(req)
```

### Available Cache Algorithms

The following built-in cache algorithms are available:

- **FIFO**: First-In-First-Out
- **LRU**: Least Recently Used
- **ARC**: Adaptive Replacement Cache
- **Clock**: Clock algorithm
- **S3FIFO**: Simple, Fast, Fair FIFO
- **Sieve**: Sieve cache algorithm
- **TinyLFU**: TinyLFU with window
- **TwoQ**: Two-Queue algorithm
- **LRB**: Learning-based cache (if enabled)
- **ThreeLCache**: Three-level cache (if enabled)

Each algorithm can be used similarly:

```python
# Examples of different cache algorithms
lru_cache = cachesim.LRU(cache_size=1024*1024)
arc_cache = cachesim.ARC(cache_size=1024*1024)
s3fifo_cache = cachesim.S3FIFO(cache_size=1024*1024)
```

### Custom Cache Implementation Example

Here's a complete example implementing a custom FIFO cache using Python hooks:

```python
import libcachesim as cachesim
from collections import deque

# Create a custom FIFO cache
cache = cachesim.PythonHookCachePolicy(cache_size=1024, cache_name="CustomFIFO")

def init_hook(cache_size):
    return deque()  # Use deque for FIFO order

def hit_hook(fifo_queue, obj_id, obj_size):
    pass  # FIFO doesn't reorder on hit

def miss_hook(fifo_queue, obj_id, obj_size):
    fifo_queue.append(obj_id)  # Add to end of queue

def eviction_hook(fifo_queue, obj_id, obj_size):
    return fifo_queue[0]  # Return first item (oldest)

def remove_hook(fifo_queue, obj_id):
    if fifo_queue and fifo_queue[0] == obj_id:
        fifo_queue.popleft()

# Set the hooks
cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

# Test the cache
req = cachesim.Request()
req.obj_id = 1
req.obj_size = 100
hit = cache.get(req)
print(f"Cache hit: {hit}")  # Should be False (miss)
```

### Testing and Validation

To ensure your custom cache implementation is correct, you can compare it against the built-in implementations:

```python
import libcachesim as cachesim

# Test your custom cache against the built-in LRU
def test_custom_vs_builtin():
    cache_size = 1024

    # Your custom LRU implementation
    custom_cache = cachesim.PythonHookCachePolicy(cache_size, "CustomLRU")
    # ... set up your LRU hooks here ...

    # Built-in LRU for comparison
    builtin_cache = cachesim.LRU(cache_size)

    # Test with same request sequence
    test_requests = [(1, 100), (2, 100), (3, 100), (1, 100)]

    for obj_id, obj_size in test_requests:
        req1 = cachesim.Request()
        req1.obj_id = obj_id
        req1.obj_size = obj_size

        req2 = cachesim.Request()
        req2.obj_id = obj_id
        req2.obj_size = obj_size

        custom_result = custom_cache.get(req1)
        builtin_result = builtin_cache.get(req2)

        assert custom_result == builtin_result, f"Mismatch at obj_id {obj_id}"
        print(f"obj_id {obj_id}: {'HIT' if custom_result else 'MISS'} ✓")
```

### Hook Function Reference

When implementing `PythonHookCachePolicy`, you need to provide these hook functions:

- **`init_hook(cache_size: int) -> Any`**: Initialize and return plugin data structure
- **`hit_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None`**: Handle cache hits
- **`miss_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None`**: Handle cache misses
- **`eviction_hook(plugin_data: Any, obj_id: int, obj_size: int) -> int`**: Return object ID to evict
- **`remove_hook(plugin_data: Any, obj_id: int) -> None`**: Clean up when object is removed
- **`free_hook(plugin_data: Any) -> None`**: [Optional] Clean up plugin resources

The `plugin_data` is whatever object you return from `init_hook()` - it can be any Python object like a list, dict, class instance, etc.

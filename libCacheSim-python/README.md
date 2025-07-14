# libCacheSim Python Binding

Python bindings for libCacheSim, a high-performance cache simulator and analysis library.

## Installation

### Quick Install (Recommended)
```bash
# From the libCacheSim root directory
bash scripts/install_python.sh
```

### Manual Install
```bash
# Build the main libCacheSim library first
cmake -G Ninja -B build
ninja -C build

# Install Python binding
cd libCacheSim-python
pip install -e . -v
```

### Testing
```bash
# Run all tests
python -m pytest .

# Test import
python -c "import libcachesim; print('Success!')"
```

## Quick Start

### Basic Usage

```python
import libcachesim as lcs

# Create a cache
cache = lcs.LRU(cache_size=1024*1024)  # 1MB cache

# Process requests
req = lcs.Request()
req.obj_id = 1
req.obj_size = 100

hit = cache.get(req)  # False (first access)
hit = cache.get(req)  # True (second access)

# Check statistics
print(f"Hit rate: {(cache.n_req - cache.n_miss)/cache.n_req:.2%}")
```

### Trace Processing

```python
import libcachesim as lcs

# Open trace and process efficiently
reader = lcs.open_trace("trace.bin", lcs.TraceType.ORACLE_GENERAL_TRACE.value)
cache = lcs.S3FIFO(cache_size=1024*1024)

# Process entire trace efficiently (C++ backend)
miss_ratio = cache.process_trace(reader)
print(f"Miss ratio: {miss_ratio:.4f}")

# Process with limits and time ranges
miss_ratio = cache.process_trace(
    reader,
    max_req=10000,      # Process max 10K requests
    max_sec=3600,       # Process max 1 hour
    start_time=1000,    # Start from timestamp 1000
    end_time=5000       # End at timestamp 5000
)
```

## Custom Cache Policies

Implement custom cache replacement algorithms using pure Python functions - no C/C++ compilation required.

### Python Hook Cache Overview

The `PythonHookCachePolicy` allows you to define custom caching behavior through Python callback functions. This is perfect for:
- Prototyping new cache algorithms
- Educational purposes and learning
- Research and experimentation
- Custom business logic implementation

### Hook Functions

You need to implement these callback functions:

- **`init_hook(cache_size: int) -> Any`**: Initialize your data structure
- **`hit_hook(data: Any, obj_id: int, obj_size: int) -> None`**: Handle cache hits
- **`miss_hook(data: Any, obj_id: int, obj_size: int) -> None`**: Handle cache misses
- **`eviction_hook(data: Any, obj_id: int, obj_size: int) -> int`**: Return object ID to evict
- **`remove_hook(data: Any, obj_id: int) -> None`**: Clean up when object removed
- **`free_hook(data: Any) -> None`**: [Optional] Final cleanup

### Example: Custom LRU Implementation

```python
import libcachesim as lcs
from collections import OrderedDict

# Create a Python hook-based cache
cache = lcs.PythonHookCachePolicy(cache_size=1024*1024, cache_name="MyLRU")

# Define LRU policy hooks
def init_hook(cache_size):
    return OrderedDict()  # Track access order

def hit_hook(lru_dict, obj_id, obj_size):
    lru_dict.move_to_end(obj_id)  # Move to most recent

def miss_hook(lru_dict, obj_id, obj_size):
    lru_dict[obj_id] = True  # Add to end

def eviction_hook(lru_dict, obj_id, obj_size):
    return next(iter(lru_dict))  # Return least recent

def remove_hook(lru_dict, obj_id):
    lru_dict.pop(obj_id, None)

# Set the hooks
cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

# Use it like any other cache
req = lcs.Request()
req.obj_id = 1
req.obj_size = 100
hit = cache.get(req)
```

### Example: Custom FIFO Implementation

```python
import libcachesim as lcs
from collections import deque

# Create a custom FIFO cache
cache = lcs.PythonHookCachePolicy(cache_size=1024, cache_name="CustomFIFO")

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

# Set the hooks and test
cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)

req = lcs.Request()
req.obj_id = 1
req.obj_size = 100
hit = cache.get(req)
print(f"Cache hit: {hit}")  # Should be False (miss)
```

## Available Algorithms

### Built-in Cache Algorithms

#### Basic Algorithms
- **FIFO**: First-In-First-Out
- **LRU**: Least Recently Used
- **LFU**: Least Frequently Used
- **Clock**: Clock/Second-chance algorithm

#### Advanced Algorithms
- **S3FIFO**: Simple, Fast, Fair FIFO (recommended for most workloads)
- **Sieve**: High-performance eviction algorithm
- **ARC**: Adaptive Replacement Cache
- **TwoQ**: Two-Queue algorithm
- **TinyLFU**: TinyLFU with window
- **SLRU**: Segmented LRU

#### Research/ML Algorithms
- **LRB**: Learning-based cache (if enabled)
- **GLCache**: Machine learning-based cache
- **ThreeLCache**: Three-level cache hierarchy (if enabled)

```python
import libcachesim as lcs

# All algorithms use the same unified interface
cache_size = 1024 * 1024  # 1MB

lru_cache = lcs.LRU(cache_size)
s3fifo_cache = lcs.S3FIFO(cache_size)      # Recommended
sieve_cache = lcs.Sieve(cache_size)
arc_cache = lcs.ARC(cache_size)

# All caches work identically
req = lcs.Request()
req.obj_id = 1
req.obj_size = 100
hit = lru_cache.get(req)
```

## Examples and Testing

### Algorithm Comparison
```python
import libcachesim as lcs

def compare_algorithms(trace_path):
    reader = lcs.open_trace(trace_path, lcs.TraceType.VSCSI_TRACE.value)
    algorithms = ['LRU', 'S3FIFO', 'Sieve', 'ARC']

    print("Algorithm\tMiss Ratio")
    print("-" * 25)
    for algo_name in algorithms:
        cache = getattr(lcs, algo_name)(cache_size=1024*1024)
        miss_ratio = cache.process_trace(reader)
        print(f"{algo_name}\t\t{miss_ratio:.4f}")

compare_algorithms("workload.vscsi")
```

### Performance Benchmarking
```python
import time

def benchmark_cache(cache, num_requests=100000):
    """Benchmark cache performance"""
    start_time = time.time()

    for i in range(num_requests):
        req = lcs.Request()
        req.obj_id = i % 1000  # Working set of 1000 objects
        req.obj_size = 100
        cache.get(req)

    end_time = time.time()
    throughput = num_requests / (end_time - start_time)

    print(f"Processed {num_requests} requests in {end_time - start_time:.2f}s")
    print(f"Throughput: {throughput:.0f} requests/sec")
    print(f"Miss ratio: {cache.n_miss / cache.n_req:.4f}")

# Compare performance
lru_cache = lcs.LRU(cache_size=1024*1024)
s3fifo_cache = lcs.S3FIFO(cache_size=1024*1024)

print("LRU Performance:")
benchmark_cache(lru_cache)

print("\nS3-FIFO Performance:")
benchmark_cache(s3fifo_cache)
```

### Validate Custom Implementation
```python
def test_custom_vs_builtin():
    """Test custom cache against built-in implementation"""
    cache_size = 1024

    # Your custom LRU implementation
    custom_cache = lcs.PythonHookCachePolicy(cache_size, "CustomLRU")
    # ... set up your LRU hooks here ...

    # Built-in LRU for comparison
    builtin_cache = lcs.LRU(cache_size)

    # Test with same request sequence
    test_requests = [(1, 100), (2, 100), (3, 100), (1, 100)]

    for obj_id, obj_size in test_requests:
        req1 = lcs.Request()
        req1.obj_id = obj_id
        req1.obj_size = obj_size

        req2 = lcs.Request()
        req2.obj_id = obj_id
        req2.obj_size = obj_size

        custom_result = custom_cache.get(req1)
        builtin_result = builtin_cache.get(req2)

        assert custom_result == builtin_result, f"Mismatch at obj_id {obj_id}"
        print(f"obj_id {obj_id}: {'HIT' if custom_result else 'MISS'} ✓")
```

## Advanced Usage

### Multi-Format Trace Processing

```python
import libcachesim as lcs

# Supported trace types
trace_types = {
    "oracle": lcs.TraceType.ORACLE_GENERAL_TRACE.value,
    "csv": lcs.TraceType.CSV_TRACE.value,
    "vscsi": lcs.TraceType.VSCSI_TRACE.value,
    "txt": lcs.TraceType.TXT_TRACE.value
}

# Open different trace formats
oracle_reader = lcs.open_trace("trace.bin", trace_types["oracle"])
csv_reader = lcs.open_trace("trace.csv", trace_types["csv"],
                           "time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,")

# Process traces with different caches
caches = [
    lcs.LRU(cache_size=1024*1024),
    lcs.S3FIFO(cache_size=1024*1024),
    lcs.Sieve(cache_size=1024*1024)
]

for i, cache in enumerate(caches):
    miss_ratio = cache.process_trace(oracle_reader)
    print(f"Cache {i} miss ratio: {miss_ratio:.4f}")
```

### Cache Hierarchy Simulation

```python
def simulate_cache_hierarchy():
    """Simulate a two-level cache hierarchy"""

    # L1 cache (small, fast)
    l1_cache = lcs.LRU(cache_size=64*1024)  # 64KB

    # L2 cache (larger, slower)
    l2_cache = lcs.LRU(cache_size=1024*1024)  # 1MB

    # Simulate requests
    total_requests = 0
    l1_hits = 0
    l2_hits = 0

    for obj_id in range(1000):
        req = lcs.Request()
        req.obj_id = obj_id % 100  # Working set of 100 objects
        req.obj_size = 1024

        total_requests += 1

        # Check L1 first
        if l1_cache.get(req):
            l1_hits += 1
        # Check L2 on L1 miss
        elif l2_cache.get(req):
            l2_hits += 1
            # Promote to L1
            l1_cache.get(req)

    print(f"L1 hit rate: {l1_hits/total_requests:.2%}")
    print(f"L2 hit rate: {l2_hits/total_requests:.2%}")
    print(f"Overall hit rate: {(l1_hits+l2_hits)/total_requests:.2%}")

simulate_cache_hierarchy()
```

### Cache Statistics Monitoring

```python
def analyze_cache_behavior():
    """Detailed cache statistics analysis"""
    cache = lcs.S3FIFO(cache_size=1024*1024)

    # Process some requests
    for i in range(1000):
        req = lcs.Request()
        req.obj_id = i % 100
        req.obj_size = 1024
        cache.get(req)

    # Access detailed statistics
    print("=== Cache Statistics ===")
    print(f"Cache size: {cache.cache_size:,} bytes")
    print(f"Occupied space: {cache.occupied_byte:,} bytes")
    print(f"Utilization: {cache.occupied_byte/cache.cache_size:.2%}")
    print(f"Objects stored: {cache.n_obj:,}")
    print(f"Total requests: {cache.n_req:,}")
    print(f"Cache hits: {cache.n_req - cache.n_miss:,}")
    print(f"Cache misses: {cache.n_miss:,}")
    print(f"Hit rate: {(cache.n_req - cache.n_miss)/cache.n_req:.2%}")
    print(f"Miss rate: {cache.n_miss/cache.n_req:.2%}")

analyze_cache_behavior()
```

## API Reference

### Unified Cache Interface

All cache policies (built-in and Python hook-based) share the same interface:

```python
import libcachesim as lcs

# All cache policies work the same way
cache = lcs.LRU(cache_size=1024*1024)
# or
cache = lcs.PythonHookCachePolicy(cache_size=1024*1024, cache_name="Custom")

# Unified methods for all caches:
req = lcs.Request()
req.obj_id = 123        # Object identifier (required)
req.obj_size = 1024     # Object size in bytes (required)
req.timestamp = 1000    # Request timestamp (optional)
req.op = 1              # Operation type (optional, default=1)

hit = cache.get(req)    # Process single request - returns True if hit, False if miss

# Batch processing (faster for large traces)
reader = lcs.open_trace("trace.bin", lcs.TraceType.ORACLE_GENERAL_TRACE.value)
miss_ratio = cache.process_trace(reader, max_req=10000)

# Unified properties for all caches:
print(f"Cache size: {cache.cache_size}")
print(f"Objects: {cache.n_obj}")
print(f"Occupied bytes: {cache.occupied_byte}")
print(f"Total requests: {cache.n_req}")
print(f"Cache misses: {cache.n_miss}")
print(f"Hit rate: {(cache.n_req - cache.n_miss) / cache.n_req:.2%}")
```

### Trace Reader

```python
# Open trace with specific format
reader = lcs.open_trace(
    trace_path="trace.csv",
    trace_type=lcs.TraceType.CSV_TRACE.value,
    trace_type_params="time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,"
)

# Process trace with options
miss_ratio = cache.process_trace(
    reader,
    max_req=10000,      # Process max requests
    max_sec=3600,       # Process max seconds of trace
    start_time=1000,    # Start from timestamp
    end_time=5000       # End at timestamp
)
```

### Supported Trace Formats
```python
# Oracle format (binary, fastest)
reader = lcs.open_trace("trace.bin", lcs.TraceType.ORACLE_GENERAL_TRACE.value)

# CSV format with custom parameters
reader = lcs.open_trace("trace.csv", lcs.TraceType.CSV_TRACE.value,
                       "time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,")

# VSCSI format
reader = lcs.open_trace("trace.vscsi", lcs.TraceType.VSCSI_TRACE.value)

# Plain text format
reader = lcs.open_trace("trace.txt", lcs.TraceType.TXT_TRACE.value)
```

### Python Hook Cache Reference

When implementing `PythonHookCachePolicy`, provide these hook functions:

```python
def init_hook(cache_size: int) -> Any:
    """Initialize and return plugin data structure"""
    return {}  # Can be any Python object

def hit_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None:
    """Handle cache hits - update your data structure"""
    pass

def miss_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None:
    """Handle cache misses - add object to your data structure"""
    pass

def eviction_hook(plugin_data: Any, obj_id: int, obj_size: int) -> int:
    """Return object ID to evict when cache is full"""
    return victim_obj_id

def remove_hook(plugin_data: Any, obj_id: int) -> None:
    """Clean up when object is removed from cache"""
    pass

def free_hook(plugin_data: Any) -> None:
    """[Optional] Final cleanup when cache is destroyed"""
    pass

# Set hooks
cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook, free_hook)
```

## Troubleshooting

### Common Issues

**Import Error**: Make sure libCacheSim C++ library is built first:
```bash
cmake -G Ninja -B build && ninja -C build
```

**Performance Issues**: Use `process_trace()` for large workloads instead of individual `get()` calls for better performance.

**Memory Usage**: Monitor cache statistics (`cache.occupied_byte`) and ensure proper cache size limits for your system.

**Custom Cache Issues**: Validate your custom implementation against built-in algorithms using the test functions above.

### Getting Help

- Check the [main documentation](/doc/) for detailed guides
- Run tests: `python -m pytest libCacheSim-python/`
- Open issues on [GitHub](https://github.com/1a1a11a/libCacheSim/issues)
- Review [examples](/example) in the main repository

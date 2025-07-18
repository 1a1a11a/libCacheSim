# libCacheSim Python Examples

This directory contains examples demonstrating how to use libCacheSim Python bindings for cache simulation and trace generation.

## Overview

libCacheSim Python bindings provide a powerful interface for:

- Cache simulation with various eviction policies (LRU, FIFO, ARC, etc.)
- Synthetic trace generation (Zipf and Uniform distributions)
- Real trace analysis and processing
- Custom cache policy implementation with Python hooks
- Unified interface supporting all cache algorithms

## Example Files

### 1. Stream Request Generation (`stream_request_example.py`)

Demonstrates how to generate synthetic request traces and use them for cache simulation:

```python
import libcachesim as lcs

# Create Zipf-distributed requests
zipf_generator = lcs.create_zipf_requests(
    num_objects=1000,      # 1000 unique objects
    num_requests=10000,    # 10000 requests
    alpha=1.0,            # Zipf skewness
    obj_size=4000,        # Object size in bytes
    seed=42               # For reproducibility
)

# Test with LRU cache
cache = lcs.LRU(cache_size=50*1024*1024)  # 50MB cache for better hit ratio
miss_count = sum(1 for req in zipf_generator if not cache.get(req))
print(f"Final miss ratio: {miss_count / 10000:.3f}")
```

**Features**:
- Memory efficient: No temporary files created
- Fast: Direct Request object generation
- Reproducible: Support for random seeds
- Flexible: Easy parameter adjustment

### 2. Unified Interface Demo (`demo_unified_interface.py`)

Shows the unified interface for all cache policies, including built-in and custom Python hook caches:

```python
import libcachesim as lcs

cache_size = 1024 * 1024  # 1MB

# Create different cache policies
caches = {
    "LRU": lcs.LRU(cache_size),
    "FIFO": lcs.FIFO(cache_size),
    "ARC": lcs.ARC(cache_size),
}

# Create Python hook cache
python_cache = lcs.PythonHookCachePolicy(cache_size, "CustomLRU")
# Set hook functions...
caches["Custom Python LRU"] = python_cache

# Unified interface testing
test_req = lcs.Request()
test_req.obj_id = 1
test_req.obj_size = 1024

for name, cache in caches.items():
    result = cache.get(test_req)
    print(f"{name}: {'HIT' if result else 'MISS'}")
```

**Benefits of Unified Interface**:
- Same API for all cache policies
- Easy to switch between different algorithms
- Efficient C++ backend trace processing
- Consistent properties and statistics

### 3. Python Hook Cache (`python_hook_cache_example.py`)

Demonstrates how to create custom cache policies using Python hooks:

```python
import libcachesim as lcs
from collections import OrderedDict

class LRUPolicy:
    def __init__(self, cache_size):
        self.access_order = OrderedDict()

    def on_hit(self, obj_id, obj_size):
        self.access_order.move_to_end(obj_id)

    def on_miss(self, obj_id, obj_size):
        self.access_order[obj_id] = True

    def evict(self, obj_id, obj_size):
        return next(iter(self.access_order))

def create_lru_cache(cache_size):
    cache = lcs.PythonHookCachePolicy(cache_size, "PythonLRU")

    def init_hook(cache_size):
        return LRUPolicy(cache_size)

    # Set other hooks...
    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    return cache
```

**Custom Policy Features**:
- Pure Python cache logic implementation
- Support for LRU, FIFO and other policies
- Flexible hook system
- Same interface as built-in policies

### 4. Zipf Trace Examples (`zipf_trace_example.py`)

Shows synthetic trace generation methods and algorithm comparison:

```python
import libcachesim as lcs

# Method 1: Create Zipf-distributed request generator
zipf_generator = lcs.create_zipf_requests(
    num_objects=1000,
    num_requests=10000,
    alpha=1.0,
    obj_size=1024,
    seed=42
)

# Method 2: Create uniform-distributed request generator
uniform_generator = lcs.create_uniform_requests(
    num_objects=1000,
    num_requests=10000,
    obj_size=1024,
    seed=42
)

# Compare different Zipf parameters
alphas = [0.5, 1.0, 1.5, 2.0]
for alpha in alphas:
    generator = lcs.create_zipf_requests(1000, 10000, alpha=alpha, seed=42)
    cache = lcs.LRU(1024*1024)
    hit_count = sum(1 for req in generator if cache.get(req))
    hit_ratio = hit_count / 10000
    print(f"α={alpha}: Hit ratio={hit_ratio:.4f}")
```

**Synthetic Trace Features**:
- Higher α values create more skewed access patterns
- Memory efficient: No temporary files created
- Request generators for flexible processing
- Suitable for simulating real workloads

## Key Features

### Trace Generation
- `create_zipf_requests()`: Create Zipf-distributed request generator
- `create_uniform_requests()`: Create uniform-distributed request generator

### Cache Algorithms
- **Classic algorithms**: `LRU()`, `FIFO()`, `ARC()`, `Clock()`
- **Modern algorithms**: `S3FIFO()`, `Sieve()`, `TinyLFU()`
- **Custom policies**: `PythonHookCachePolicy()`

### Trace Processing
- `open_trace()`: Open real trace files
- `process_trace()`: High-performance trace processing

## Basic Usage Examples

### 1. Compare Cache Algorithms

```python
import libcachesim as lcs

# Test different algorithms
algorithms = ['LRU', 'FIFO', 'ARC', 'S3FIFO']
cache_size = 1024*1024

for algo_name in algorithms:
    # Create fresh workload for each algorithm
    generator = lcs.create_zipf_requests(1000, 10000, alpha=1.0, seed=42)
    cache = getattr(lcs, algo_name)(cache_size)
    hit_count = sum(1 for req in generator if cache.get(req))
    print(f"{algo_name}: {hit_count/10000:.3f}")
```

### 2. Parameter Sensitivity Analysis

```python
import libcachesim as lcs

# Test different Zipf parameters
for alpha in [0.5, 1.0, 1.5, 2.0]:
    generator = lcs.create_zipf_requests(1000, 10000, alpha=alpha, seed=42)
    cache = lcs.LRU(cache_size=512*1024)

    hit_count = sum(1 for req in generator if cache.get(req))
    print(f"α={alpha}: Hit ratio={hit_count/10000:.3f}")
```

## Parameters

### Trace Generation Parameters
- `num_objects`: Number of unique objects
- `num_requests`: Number of requests to generate
- `alpha`: Zipf skewness (α=1.0 for classic Zipf)
- `obj_size`: Object size in bytes (default: 4000)
- `seed`: Random seed for reproducibility

### Cache Parameters
- `cache_size`: Cache capacity in bytes
- Algorithm-specific parameters (e.g.,`fifo_size_ratio` for S3FIFO)

## Running Examples

```bash
# Navigate to examples directory
cd libCacheSim-python/examples

# Run stream-based trace generation
python stream_request_example.py

# Run unified interface demo
python demo_unified_interface.py

# Run Python hook cache example
python python_hook_cache_example.py

# Run Zipf trace examples
python zipf_trace_example.py

# Run all tests
python -m pytest ../tests/ -v
```

## Performance Tips

1. **Use appropriate cache and object sizes**:
   ```python
   # Good: cache can hold multiple objects
   cache = lcs.LRU(cache_size=1024*1024)  # 1MB
   generator = lcs.create_zipf_requests(1000, 10000, obj_size=1024)  # 1KB objects
   ```

2. **Use seeds for reproducible experiments**:
   ```python
   generator = lcs.create_zipf_requests(1000, 10000, seed=42)
   ```

3. **Process large traces with C++ backend**:
   ```python
   # Fast: C++ processing
   obj_miss_ratio, byte_miss_ratio = lcs.process_trace(cache, reader)

   # Slow: Python loop
   for req in reader:
       cache.get(req)
   ```

4. **Understand Zipf parameter effects**:
   - α=0.5: Slightly skewed, close to uniform distribution
   - α=1.0: Classic Zipf distribution
   - α=2.0: Highly skewed, few objects get most accesses

## Testing

Run comprehensive tests:

```bash
python -m pytest ../tests/test_trace_generator.py -v
python -m pytest ../tests/test_eviction.py -v
python -m pytest ../tests/test_process_trace.py -v
```

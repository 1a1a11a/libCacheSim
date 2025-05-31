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

## Features

- [x] Support for multiple eviction policies (FIFO, LRU, ARC, Clock, etc.)
- [ ] trace analysis tools

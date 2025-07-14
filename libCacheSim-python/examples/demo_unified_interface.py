#!/usr/bin/env python3
"""
Demo script showing the unified interface for all cache policies.
This demonstrates how to use both native and Python hook-based caches
with the same API for seamless algorithm comparison and switching.
"""

import sys
import os

# Add parent directory for development testing
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import libcachesim as lcs
except ImportError as e:
    print(f"Error importing libcachesim: {e}")
    print("Make sure the Python binding is built and installed")
    sys.exit(1)

from collections import OrderedDict


def create_trace_reader():
    """Helper function to create a trace reader."""
    data_file = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
        "data",
        "cloudPhysicsIO.oracleGeneral.bin"
    )
    if not os.path.exists(data_file):
        print(f"Warning: Trace file not found at {data_file}")
        return None
    return lcs.open_trace(data_file, lcs.TraceType.ORACLE_GENERAL_TRACE)


def create_demo_lru_hooks():
    """Create demo LRU hooks for Python-based cache policy."""

    def init_hook(cache_size):
        print(f"  Initializing custom LRU with {cache_size} bytes")
        return OrderedDict()

    def hit_hook(lru_dict, obj_id, obj_size):
        if obj_id in lru_dict:
            lru_dict.move_to_end(obj_id)

    def miss_hook(lru_dict, obj_id, obj_size):
        lru_dict[obj_id] = obj_size

    def eviction_hook(lru_dict, obj_id, obj_size):
        if lru_dict:
            return next(iter(lru_dict))
        return obj_id

    def remove_hook(lru_dict, obj_id):
        lru_dict.pop(obj_id, None)

    return init_hook, hit_hook, miss_hook, eviction_hook, remove_hook


def demo_unified_interface():
    """Demonstrate the unified interface across different cache policies."""
    print("libCacheSim Python Binding - Unified Interface Demo")
    print("=" * 60)

    cache_size = 1024 * 1024  # 1MB

    # Create different cache policies
    caches = {
        "LRU": lcs.LRU(cache_size),
        "FIFO": lcs.FIFO(cache_size),
        "ARC": lcs.ARC(cache_size),
    }

    # Create Python hook-based LRU
    python_cache = lcs.PythonHookCachePolicy(cache_size, "CustomLRU")
    init_hook, hit_hook, miss_hook, eviction_hook, remove_hook = create_demo_lru_hooks()
    python_cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    caches["Custom Python LRU"] = python_cache

    print(f"Testing {len(caches)} different cache policies with unified interface:")

    # Demo 1: Single request interface
    print("1. Single Request Interface:")
    print("   All caches use: cache.get(request)")

    test_req = lcs.Request()
    test_req.obj_id = 1
    test_req.obj_size = 1024

    for name, cache in caches.items():
        result = cache.get(test_req)
        print(f"   {name:20s}: {'HIT' if result else 'MISS'}")

    # Demo 2: Unified properties interface
    print("\n2. Unified Properties Interface:")
    print("   All caches provide: cache_size, n_obj, occupied_byte, n_req")

    for name, cache in caches.items():
        print(f"   {name:20s}: size={cache.cache_size}, objs={cache.n_obj}, "
              f"bytes={cache.occupied_byte}, reqs={cache.n_req}")

    # Demo 3: Efficient trace processing
    print("\n3. Efficient Trace Processing Interface:")
    print("   All caches use: cache.process_trace(reader, max_req=N)")

    max_requests = 1000

    for name, cache in caches.items():
        # Create fresh reader for each cache
        reader = create_trace_reader()
        if not reader:
            print(f"   {name:20s}: trace file not available")
            continue

        miss_ratio = cache.process_trace(reader, max_req=max_requests)
        print(f"   {name:20s}: miss_ratio={miss_ratio:.4f}")

    print("\nKey Benefits of Unified Interface:")
    print("   • Same API for all cache policies (built-in + custom)")
    print("   • Easy to switch between different algorithms")
    print("   • Efficient trace processing in C++ (no Python overhead)")
    print("   • Consistent properties and statistics")
    print("   • Type-safe and well-documented")

    print("\nDemo completed! All cache policies work with the same interface.")


if __name__ == "__main__":
    demo_unified_interface()

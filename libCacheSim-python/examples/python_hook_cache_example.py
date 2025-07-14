#!/usr/bin/env python3
"""
Example demonstrating how to create custom cache policies using Python hooks.

This example shows how to implement LRU and FIFO cache policies using the
PythonHookCachePolicy class, which allows users to define cache behavior using
pure Python functions instead of C/C++ plugins.
"""

import libcachesim as lcs
from collections import OrderedDict, deque


class LRUPolicy:
    """LRU (Least Recently Used) cache policy implementation."""

    def __init__(self, cache_size):
        self.cache_size = cache_size
        self.access_order = OrderedDict()  # obj_id -> True (for ordering)

    def on_hit(self, obj_id, obj_size):
        """Move accessed object to end (most recent)."""
        if obj_id in self.access_order:
            # Move to end (most recent)
            self.access_order.move_to_end(obj_id)

    def on_miss(self, obj_id, obj_size):
        """Add new object to end (most recent)."""
        self.access_order[obj_id] = True

    def evict(self, obj_id, obj_size):
        """Return the least recently used object ID."""
        if self.access_order:
            # Return first item (least recent)
            victim_id = next(iter(self.access_order))
            return victim_id
        raise RuntimeError("No objects to evict")

    def on_remove(self, obj_id):
        """Remove object from tracking."""
        self.access_order.pop(obj_id, None)


class FIFOPolicy:
    """FIFO (First In First Out) cache policy implementation."""

    def __init__(self, cache_size):
        self.cache_size = cache_size
        self.insertion_order = deque()  # obj_id queue

    def on_hit(self, obj_id, obj_size):
        """FIFO doesn't change order on hits."""
        pass

    def on_miss(self, obj_id, obj_size):
        """Add new object to end of queue."""
        self.insertion_order.append(obj_id)

    def evict(self, obj_id, obj_size):
        """Return the first inserted object ID."""
        if self.insertion_order:
            victim_id = self.insertion_order.popleft()
            return victim_id
        raise RuntimeError("No objects to evict")

    def on_remove(self, obj_id):
        """Remove object from tracking."""
        try:
            self.insertion_order.remove(obj_id)
        except ValueError:
            pass  # Object not in queue


def create_lru_cache(cache_size):
    """Create an LRU cache using Python hooks."""
    cache = lcs.PythonHookCachePolicy(cache_size, "PythonLRU")

    def init_hook(cache_size):
        return LRUPolicy(cache_size)

    def hit_hook(policy, obj_id, obj_size):
        policy.on_hit(obj_id, obj_size)

    def miss_hook(policy, obj_id, obj_size):
        policy.on_miss(obj_id, obj_size)

    def eviction_hook(policy, obj_id, obj_size):
        return policy.evict(obj_id, obj_size)

    def remove_hook(policy, obj_id):
        policy.on_remove(obj_id)

    def free_hook(policy):
        # Python garbage collection handles cleanup
        pass

    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook, free_hook)
    return cache


def create_fifo_cache(cache_size):
    """Create a FIFO cache using Python hooks."""
    cache = lcs.PythonHookCachePolicy(cache_size, "PythonFIFO")

    def init_hook(cache_size):
        return FIFOPolicy(cache_size)

    def hit_hook(policy, obj_id, obj_size):
        policy.on_hit(obj_id, obj_size)

    def miss_hook(policy, obj_id, obj_size):
        policy.on_miss(obj_id, obj_size)

    def eviction_hook(policy, obj_id, obj_size):
        return policy.evict(obj_id, obj_size)

    def remove_hook(policy, obj_id):
        policy.on_remove(obj_id)

    cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
    return cache


def test_cache_policy(cache, name):
    """Test a cache policy with sample requests."""
    print(f"\n=== Testing {name} Cache ===")

    # Test requests: obj_id, obj_size
    test_requests = [
        (1, 100), (2, 100), (3, 100), (4, 100), (5, 100),  # Fill cache
        (1, 100),  # Hit
        (6, 100),  # Miss, should evict something
        (2, 100),  # Hit or miss depending on policy
        (7, 100),  # Miss, should evict something
    ]

    hits = 0
    misses = 0

    for obj_id, obj_size in test_requests:
        req = lcs.Request()
        req.obj_id = obj_id
        req.obj_size = obj_size

        hit = cache.get(req)
        if hit:
            hits += 1
            print(f"Request {obj_id}: HIT")
        else:
            misses += 1
            print(f"Request {obj_id}: MISS")

    print(f"Total: {hits} hits, {misses} misses")
    print(f"Cache stats: {cache.n_obj} objects, {cache.occupied_byte} bytes occupied")


def main():
    """Main example function."""
    cache_size = 400  # Bytes (can hold 4 objects of size 100 each)

    # Test LRU cache
    lru_cache = create_lru_cache(cache_size)
    test_cache_policy(lru_cache, "LRU")

    # Test FIFO cache
    fifo_cache = create_fifo_cache(cache_size)
    test_cache_policy(fifo_cache, "FIFO")

    print("\n=== Comparison ===")
    print("LRU keeps recently accessed items, evicting least recently used")
    print("FIFO keeps items in insertion order, evicting oldest inserted")


if __name__ == "__main__":
    main()
